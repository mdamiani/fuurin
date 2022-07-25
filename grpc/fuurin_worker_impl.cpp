/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin_worker_impl.h"

#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"
#include "fuurin/zmqcancel.h"
#include "fuurin/worker.h"
#include "fuurin/workerconfig.h"
#include "fuurin/uuid.h"
#include "fuurin/logger.h"
#include "fuurin/zmqtimer.h"

#include <grpcpp/server_builder.h>

#include <string_view>
#include <string>
#include <optional>
#include <vector>


namespace flog = fuurin::log;

using namespace std::literals::string_view_literals;
using namespace std::literals::string_literals;
using namespace std::literals::chrono_literals;


namespace {
template<typename... Args>
void log_fatal(Args&&... args)
{
    flog::Arg arr[] = {args...};
    flog::Logger::fatal({__FILE__, __LINE__}, arr, sizeof...(Args));
}


template<typename T>
fuurin::zmq::Part protoSerialize(const T* val)
{
    fuurin::zmq::Part ret{fuurin::zmq::Part::msg_init_size, val->ByteSizeLong()};
    val->SerializeToArray(ret.data(), ret.size());
    return ret;
}


template<typename T>
T protoParse(std::string_view name, const fuurin::zmq::Part& val)
{
    T ret;
    if (!ret.ParseFromArray(val.data(), val.size()))
        log_fatal(flog::Arg(name), flog::Arg("error"sv, "failed ParseFromArray"sv));
    return ret;
}

} // namespace


WorkerServiceImpl::WorkerServiceImpl(const std::string& server_addr)
    : server_addr_{server_addr}
    , worker_{std::make_unique<fuurin::Worker>()}
    , zrpcClient_{std::make_unique<fuurin::zmq::Socket>(worker_->context(), fuurin::zmq::Socket::CLIENT)}
    , zrpcServer_{std::make_unique<fuurin::zmq::Socket>(worker_->context(), fuurin::zmq::Socket::SERVER)}
    , zrpcEvents_{std::make_unique<fuurin::zmq::Socket>(worker_->context(), fuurin::zmq::Socket::RADIO)}
    , zcanc1_{std::make_unique<fuurin::zmq::Cancellation>(worker_->context(), "WorkerServiceImpl_canc1")}
    , zcanc2_{std::make_unique<fuurin::zmq::Cancellation>(worker_->context(), "WorkerServiceImpl_canc2")}
    , cancNum_{0}
{
    zrpcEvents_->setEndpoints({"inproc://rpc-events"});
    zrpcServer_->setEndpoints({"inproc://rpc-worker"});
    zrpcClient_->setEndpoints({"inproc://rpc-worker"});

    zrpcEvents_->bind();
    zrpcServer_->bind();
    zrpcClient_->connect();

    client_ = std::async(std::launch::async, &WorkerServiceImpl::runClient, this);
    events_ = std::async(std::launch::async, &WorkerServiceImpl::runEvents, this);
}


WorkerServiceImpl::~WorkerServiceImpl() noexcept
{
    sendRPC(RPC::Cancel);
    client_.get();
    events_.get();
}


auto WorkerServiceImpl::Run(const std::string& addr, const utils::Endpoints& endp) -> std::tuple<
    std::unique_ptr<WorkerServiceImpl>,
    std::future<void>,
    CancelFn,
    utils::Endpoints,
    bool>
{
    auto service = new WorkerServiceImpl{addr};
    auto retEndp = utils::applyArgsEndpoints(endp, service->worker_.get());

    auto cancel = std::bind(&WorkerServiceImpl::shutdown, service);
    auto promise = std::promise<bool>{};
    auto started = promise.get_future();
    auto future = std::async(std::launch::async, &WorkerServiceImpl::runServer, service, &promise);

    // wait for server started
    auto succ = started.get();

    return {
        std::unique_ptr<WorkerServiceImpl>{service},
        std::move(future),
        std::move(cancel),
        retEndp,
        succ,
    };
}


void WorkerServiceImpl::shutdown()
{
    zcanc2_->cancel();

    if (server_)
        server_->Shutdown();

    sendRPC(RPC::SetStop);
}


grpc::Status WorkerServiceImpl::GetUuid(grpc::ServerContext*,
    const google::protobuf::Empty*, Uuid* resp)
{
    sendRPC(RPC::GetUuid);

    const auto bytes = fuurin::Uuid::fromPart(recvRPC()).bytes();
    resp->set_data(bytes.data(), bytes.size());

    return grpc::Status::OK;
}


grpc::Status WorkerServiceImpl::GetSeqNum(grpc::ServerContext*,
    const google::protobuf::Empty*, SeqNum* resp)
{
    sendRPC(RPC::GetSeqNum);

    const auto seqn = recvRPC().toUint64();
    resp->set_value(seqn);

    return grpc::Status::OK;
}


grpc::Status WorkerServiceImpl::SetEndpoints(grpc::ServerContext*,
    const Endpoints* endp, google::protobuf::Empty*)
{
    sendRPC(RPC::SetEndpoints, protoSerialize(endp));

    return grpc::Status::OK;
}


grpc::Status WorkerServiceImpl::SetSubscriptions(grpc::ServerContext*,
    const Subscriptions* subscr, google::protobuf::Empty*)
{
    sendRPC(RPC::SetSubscriptions, protoSerialize(subscr));

    return grpc::Status::OK;
}


grpc::Status WorkerServiceImpl::Start(grpc::ServerContext*,
    const google::protobuf::Empty*, google::protobuf::Empty*)
{
    sendRPC(RPC::SetStart);

    return grpc::Status::OK;
}


grpc::Status WorkerServiceImpl::Stop(grpc::ServerContext*,
    const google::protobuf::Empty*, google::protobuf::Empty*)
{
    sendRPC(RPC::SetStop);

    return grpc::Status::OK;
}


grpc::Status WorkerServiceImpl::Sync(grpc::ServerContext*,
    const google::protobuf::Empty*, google::protobuf::Empty*)
{
    sendRPC(RPC::SetSync);

    return grpc::Status::OK;
}


grpc::Status WorkerServiceImpl::Dispatch(grpc::ServerContext*,
    grpc::ServerReader<Topic>* stream, google::protobuf::Empty*)
{
    Topic topic;

    while (stream->Read(&topic)) {
        sendRPC(RPC::SetDispatch, protoSerialize(&topic));
    }

    return grpc::Status::OK;
}


grpc::Status WorkerServiceImpl::WaitForEvent(grpc::ServerContext* context,
    const EventTimeout* timeout, grpc::ServerWriter<Event>* stream)
{
    fuurin::zmq::Socket zevDish{worker_->context(), fuurin::zmq::Socket::DISH};
    zevDish.setEndpoints({"inproc://rpc-events"});
    zevDish.setGroups({"EVNT"});
    zevDish.connect();

    fuurin::zmq::Cancellation tmeoCanc{worker_->context(),
        "WorkerServiceImpl::WaitForEvent_canc#"s + std::to_string(++cancNum_)};

    if (timeout->millis() > 0)
        tmeoCanc.setDeadline(std::chrono::milliseconds{timeout->millis()});

    fuurin::zmq::Poller poll{fuurin::zmq::PollerEvents::Type::Read, LatencyDuration,
        &zevDish, zcanc2_.get(), &tmeoCanc};

    const auto createEvent = [](Event_Type type) {
        Event e;
        e.set_type(type);
        return e;
    };

    bool rcpSetupSent = false;

    grpc::Status ret;

    for (;;) {
        const auto sock = poll.wait();

        if (context->IsCancelled())
            return grpc::Status::CANCELLED;

        for (auto s : sock) {
            if (s == zcanc2_.get()) {
                stream->Write(createEvent(Event_Type_RCPTeardown));
                return grpc::Status::CANCELLED;
            }

            if (s == &tmeoCanc) {
                stream->Write(createEvent(Event_Type_RCPTeardown));
                return grpc::Status::OK;
            }

            fuurin::zmq::Part pay;
            zevDish.recv(&pay);

            const auto ev = getEvent(pay);

            /**
             * Either a liveness ping or an actual event
             * from the channel, so it's alive.
             * Thus send an Event::RCPSetup at first.
             */
            if (!rcpSetupSent) {
                rcpSetupSent = true;
                stream->Write(createEvent(Event_Type_RCPSetup));
            }

            if (ev) {
                stream->Write(ev.value());
            }
        }
    }

    return grpc::Status{grpc::INTERNAL, "internal error"};
}


void WorkerServiceImpl::runServer(std::promise<bool>* started)
{
    grpc::ServerBuilder builder;

    builder.AddListeningPort(server_addr_, grpc::InsecureServerCredentials());
    builder.RegisterService(this);

    server_ = builder.BuildAndStart();

    started->set_value(server_ != nullptr);

    if (server_)
        server_->Wait();
}


void WorkerServiceImpl::runClient()
{
    fuurin::zmq::Timer mon{worker_->context(), "WorkerServiceImpl::runClient_monitor"};
    mon.setInterval(LatencyDuration);
    mon.start();

    fuurin::zmq::Poller poll{fuurin::zmq::PollerEvents::Type::Read,
        zrpcServer_.get(), zcanc1_.get(), &mon};

    for (;;) {
        for (auto s : poll.wait()) {
            if (s == zcanc1_.get())
                return;

            if (s == &mon) {
                mon.consume();
                utils::waitForResult(&active_, 0ms);
                continue;
            }

            fuurin::zmq::Part r;
            zrpcServer_->recv(&r);

            auto [rpc, pay] = fuurin::zmq::PartMulti::unpack<RPCType, fuurin::zmq::Part>(r);

            auto reply = serveRPC(RPC{rpc}, pay);

            if (!reply.empty())
                zrpcServer_->send(reply.withRoutingID(r.routingID()));
        }
    }
}


void WorkerServiceImpl::runEvents()
{
    fuurin::zmq::Poller poll{fuurin::zmq::PollerEvents::Type::Read, 0ms, zcanc1_.get()};

    for (;;) {
        const auto& ev = worker_->waitForEvent(LatencyDuration / 2);

        /**
         * In case timeout has expired, then we get an event `ev` such that:
         * ev.notification() == fuurin::Event::Notification::Timeout.
         * It's ok to forward this timeout that shall act as a liveness ping.
         */
        zrpcEvents_->send(ev.toPart().withGroup("EVNT"));

        /**
         * Checking here for global cancellation implies waiting for a time
         * proporitional to LatencyDuration, before reacting to such command.
         *
         * TODO: let worker_->waitForEvent() exit immediately as soon as zcanc1_ is cancelled.
         */
        if (!poll.wait().empty())
            return;
    }
}


void WorkerServiceImpl::sendRPC(RPC type)
{
    sendRPC(type, fuurin::zmq::Part{});
}


void WorkerServiceImpl::sendRPC(RPC type, fuurin::zmq::Part&& pay)
{
    zrpcClient_->send(fuurin::zmq::PartMulti::pack(static_cast<RPCType>(type), std::move(pay)));
}


fuurin::zmq::Part WorkerServiceImpl::recvRPC()
{
    fuurin::zmq::Part reply;
    zrpcClient_->recv(&reply);

    return reply;
}


fuurin::zmq::Part WorkerServiceImpl::serveRPC(RPC type, const fuurin::zmq::Part& pay)
{
    fuurin::zmq::Part ret;

    switch (type) {
    case RPC::Cancel:
        zcanc1_->cancel();
        break;

    case RPC::GetUuid:
        ret = worker_->uuid().toPart();
        break;

    case RPC::GetSeqNum:
        ret = fuurin::zmq::Part{worker_->seqNumber()};
        break;

    case RPC::SetEndpoints:
        applyEndpoints(protoParse<Endpoints>("RPC::SetEndpoints"sv, pay));
        break;

    case RPC::SetSubscriptions:
        applySubscriptions(protoParse<Subscriptions>("RPC::SetSubscriptions"sv, pay));
        break;

    case RPC::SetStart:
        if (!worker_->isRunning())
            active_ = worker_->start();
        break;

    case RPC::SetStop:
        if (worker_->isRunning()) {
            worker_->stop();
        }
        utils::waitForResult(&active_);
        break;

    case RPC::SetSync:
        worker_->sync();
        break;

    case RPC::SetDispatch:
        setDispatch(protoParse<Topic>("RPC::SetDispatch"sv, pay));
        break;
    };

    return ret;
}


void WorkerServiceImpl::applySubscriptions(const Subscriptions& subscr)
{
    if (subscr.wildcard()) {
        worker_->setTopicsAll();
        return;
    }

    const size_t size = subscr.name_size();
    std::vector<fuurin::Topic::Name> topics{size};

    for (size_t i = 0; i < size; ++i)
        topics[i] = subscr.name(i);

    worker_->setTopicsNames(topics);
}


void WorkerServiceImpl::applyEndpoints(const Endpoints& endp)
{
    std::vector<std::string> delivery, dispatch, snapshot;

    for (int i = 0; i < endp.delivery_size(); ++i) {
        delivery.push_back(endp.delivery(i));
    }
    for (int i = 0; i < endp.dispatch_size(); ++i) {
        dispatch.push_back(endp.dispatch(i));
    }
    for (int i = 0; i < endp.snapshot_size(); ++i) {
        snapshot.push_back(endp.snapshot(i));
    }

    worker_->setEndpoints(delivery, dispatch, snapshot);
}


void WorkerServiceImpl::setDispatch(const Topic& topic)
{
    worker_->dispatch(fuurin::Topic::Name{topic.name()},
        fuurin::Topic::Data{topic.data()},
        topic.type() == Topic_Type_State
            ? fuurin::Topic::State
            : fuurin::Topic::Event);
}

std::optional<Event> WorkerServiceImpl::getEvent(const fuurin::zmq::Part& pay) const
{
    // TODO: event payload is here copied, we could just use a view over 'pay'.
    const auto ev = fuurin::Event::fromPart(pay);

    if (ev.notification() == fuurin::Event::Notification::Timeout ||
        ev.notification() == fuurin::Event::Notification::Discard ||
        ev.type() == fuurin::Event::Type::Invalid) //
    {
        return {};
    }

    Event ret;

    const auto fillConfig = [](const fuurin::Event& ev, ConfigEvent* cfg) {
        const auto wc = fuurin::WorkerConfig::fromPart(ev.payload());

        cfg->mutable_uuid()->set_data(wc.uuid.bytes().data(), wc.uuid.bytes().size());
        cfg->mutable_seqn()->set_value(wc.seqNum);

        auto cfgendp = cfg->mutable_endpoints();

        for (const auto& endp : wc.endpDelivery)
            cfgendp->add_delivery(endp);

        for (const auto& endp : wc.endpDispatch)
            cfgendp->add_dispatch(endp);

        for (const auto& endp : wc.endpSnapshot)
            cfgendp->add_snapshot(endp);

        auto subscr = cfg->mutable_subscriptions();

        subscr->set_wildcard(wc.topicsAll);

        for (const auto& name : wc.topicsNames)
            subscr->add_name(name);
    };

    const auto fillTopic = [](const fuurin::Event& ev, TopicEvent* tpc) {
        const auto t = fuurin::Topic::fromPart(ev.payload());

        tpc->mutable_seqn()->set_value(t.seqNum());
        tpc->mutable_broker()->set_data(t.broker().bytes().data(), t.broker().bytes().size());
        tpc->mutable_worker()->set_data(t.worker().bytes().data(), t.worker().bytes().size());

        switch (t.type()) {
        case fuurin::Topic::Type::State:
            tpc->mutable_topic()->set_type(Topic_Type_State);
            break;

        case fuurin::Topic::Type::Event:
            tpc->mutable_topic()->set_type(Topic_Type_Event);
            break;
        };

        tpc->mutable_topic()->set_name(t.name());
        tpc->mutable_topic()->set_data(std::string(t.data().toString()));
    };

    switch (ev.type()) {
    case fuurin::Event::Type::Started:
        ret.set_type(Event_Type_Started);
        fillConfig(ev, ret.mutable_configevent());
        break;

    case fuurin::Event::Type::Stopped:
        ret.set_type(Event_Type_Stopped);
        break;

    case fuurin::Event::Type::Offline:
        ret.set_type(Event_Type_Offline);
        break;

    case fuurin::Event::Type::Online:
        ret.set_type(Event_Type_Online);
        break;

    case fuurin::Event::Type::Delivery:
        ret.set_type(Event_Type_Delivery);
        fillTopic(ev, ret.mutable_topicevent());
        break;

    case fuurin::Event::Type::SyncRequest:
        ret.set_type(Event_Type_SyncRequest);
        fillConfig(ev, ret.mutable_configevent());
        break;

    case fuurin::Event::Type::SyncBegin:
        ret.set_type(Event_Type_SyncBegin);
        break;

    case fuurin::Event::Type::SyncElement:
        ret.set_type(Event_Type_SyncElement);
        fillTopic(ev, ret.mutable_topicevent());
        break;

    case fuurin::Event::Type::SyncSuccess:
        ret.set_type(Event_Type_SyncSuccess);
        break;

    case fuurin::Event::Type::SyncError:
        ret.set_type(Event_Type_SyncError);
        break;

    case fuurin::Event::Type::SyncDownloadOn:
        ret.set_type(Event_Type_SyncDownloadOn);
        break;

    case fuurin::Event::Type::SyncDownloadOff:
        ret.set_type(Event_Type_SyncDownloadOff);
        break;

    case fuurin::Event::Type::Invalid:
    case fuurin::Event::Type::COUNT:
        log_fatal(flog::Arg{"event type is unexpectedly invalid"sv});
        break;
    };

    return {ret};
}
