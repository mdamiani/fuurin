/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/worker.h"
#include "fuurin/errors.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"
#include "fuurin/zmqtimer.h"
#include "connmachine.h"
#include "syncmachine.h"
#include "types.h"
#include "log.h"

#include <algorithm>
#include <utility>
#include <chrono>
#include <list>
#include <type_traits>


#define BROKER_HUGZ "HUGZ"
#define WORKER_HUGZ "HUGZ"
#define BROKER_UPDT "UPDT"
#define WORKER_UPDT "UPDT"

#define BROKER_SYNC_REQST "SYNC"sv
#define BROKER_SYNC_BEGIN "BEGN"sv
#define BROKER_SYNC_ELEMN "ELEM"sv
#define BROKER_SYNC_COMPL "SONC"sv


using namespace std::literals::string_view_literals;


namespace fuurin {

/**
 * MAIN TASK
 */

Worker::Worker(Uuid id, Topic::SeqN initSequence)
    : Runner{id}
    , seqNum_{initSequence}
    , zseqs_(std::make_unique<zmq::Socket>(zmqCtx(), zmq::Socket::PUSH))
    , zseqr_(std::make_unique<zmq::Socket>(zmqCtx(), zmq::Socket::PULL))
{
    // MUST be inproc in order to get instant delivery of messages.
    zseqs_->setEndpoints({"inproc://worker-seqn"});
    zseqr_->setEndpoints({"inproc://worker-seqn"});

    zseqs_->setHighWaterMark(1, 1);
    zseqr_->setHighWaterMark(1, 1);

    zseqs_->setConflate(true);
    zseqr_->setConflate(true);

    zseqr_->bind();
    zseqs_->connect();
}


Worker::~Worker() noexcept
{
}


void Worker::setTopicNames(const std::vector<Topic::Name>& names)
{
    names_ = names;
}


void Worker::dispatch(Topic::Name name, const Topic::Data& data)
{
    dispatch(name, zmq::Part{data});
}


void Worker::dispatch(Topic::Name name, Topic::Data& data)
{
    dispatch(name, std::move(data));
}


void Worker::dispatch(Topic::Name name, Topic::Data&& data)
{
    if (!isRunning())
        return;

    sendOperation(Operation::Type::Dispatch,
        Topic{Uuid{}, uuid(), Topic::SeqN{}, name, data}.toPart());
}


void Worker::sync()
{
    if (!isRunning())
        return;

    sendOperation(Operation::Type::Sync);
}


Event Worker::waitForEvent(std::chrono::milliseconds timeout)
{
    const auto& ev = Runner::waitForEvent(timeout);

    LOG_DEBUG(log::Arg{"worker"sv, uuid().toShortString()},
        log::Arg{"event"sv, Event::toString(ev.notification())},
        log::Arg{"type"sv, Event::toString(ev.type())},
        log::Arg{"size"sv, int(ev.payload().size())});

    return ev;
}


Topic::SeqN Worker::seqNumber() const
{
    // latest message is always available
    // over the socket, because we are using
    // inproc transport.
    zmq::Part r;
    if (zseqr_->tryRecv(&r) != -1)
        seqNum_ = r.toUint64();

    return seqNum_;
}


zmq::Part Worker::prepareConfiguration() const
{
    return WorkerConfig{uuid(), seqNumber(), names_}.toPart();
}


std::unique_ptr<Runner::Session> Worker::createSession(CompletionFunc onComplete) const
{
    return makeSession<WorkerSession>(onComplete, zseqs_.get());
}


/**
 * ASYNC TASK
 */

Worker::WorkerSession::WorkerSession(Uuid id, token_type_t token, CompletionFunc onComplete,
    zmq::Context* zctx, zmq::Socket* zoper, zmq::Socket* zevent, zmq::Socket* zseqs)
    : Session(id, token, onComplete, zctx, zoper, zevent)
    , zsnapshot_{std::make_unique<zmq::Socket>(zctx, zmq::Socket::CLIENT)}
    , zdelivery_{std::make_unique<zmq::Socket>(zctx, zmq::Socket::DISH)}
    , zdispatch_{std::make_unique<zmq::Socket>(zctx, zmq::Socket::RADIO)}
    , conn_{
          std::make_unique<ConnMachine>("worker"sv, id, zctx,
              500ms,  // TODO: make configurable
              3000ms, // TODO: make configurable

              std::bind(&Worker::WorkerSession::connClose, this),    //
              std::bind(&Worker::WorkerSession::connOpen, this),     //
              std::bind(&Worker::WorkerSession::sendAnnounce, this), //

              [this](ConnMachine::State s) {
                  switch (s) {
                  case ConnMachine::State::Halted:
                  case ConnMachine::State::Trying:
                      notifyConnectionUpdate(false);
                      break;

                  case ConnMachine::State::Stable:
                      notifyConnectionUpdate(true);
                      break;
                  };
              }),
      }
    , sync_{
          std::make_unique<SyncMachine>("worker"sv, id, zctx,
              0,      // TODO: depends on number of endpoints
              1,      // TODO: make configurable
              3000ms, // TODO: make configurable

              std::bind(&Worker::WorkerSession::snapClose, this),                       //
              std::bind(&Worker::WorkerSession::snapOpen, this),                        //
              std::bind(&Worker::WorkerSession::sendSync, this, std::placeholders::_2), //

              [this](SyncMachine::State s) {
                  switch (s) {
                  case SyncMachine::State::Halted:
                      if (isSnapshot_) {
                          LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()},
                              log::Arg{"snapshot"sv, "halt"sv},
                              log::Arg{"broker", brokerUuid_.toShortString()},
                              log::Arg{"status"sv, Event::toString(Event::Type::SyncError)});

                          sendEvent(Event::Type::SyncError, brokerUuid_.toPart());
                      }
                      brokerUuid_ = Uuid{};
                      notifySnapshotDownload(false);
                      break;

                  case SyncMachine::State::Synced:
                      LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()},
                          log::Arg{"snapshot"sv, "recv"sv},
                          log::Arg{"broker", brokerUuid_.toShortString()},
                          log::Arg{"status"sv, Event::toString(Event::Type::SyncSuccess)});

                      sendEvent(Event::Type::SyncSuccess, brokerUuid_.toPart());
                      notifySnapshotDownload(false);
                      break;

                  case SyncMachine::State::Failed:
                      LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()},
                          log::Arg{"snapshot"sv, "timeout"sv},
                          log::Arg{"broker", brokerUuid_.toShortString()},
                          log::Arg{"status"sv, Event::toString(Event::Type::SyncError)});

                      sendEvent(Event::Type::SyncError, brokerUuid_.toPart());
                      notifySnapshotDownload(false);
                      break;

                  case SyncMachine::State::Download:
                      notifySnapshotDownload(true);
                      break;
                  };
              }),
      }
    , zseqs_{zseqs}
    , isOnline_{false}
    , isSnapshot_{false}
{
}


Worker::WorkerSession::~WorkerSession() noexcept = default;


bool Worker::WorkerSession::isOnline() const noexcept
{
    return isOnline_;
}


std::unique_ptr<zmq::PollerWaiter> Worker::WorkerSession::createPoller()
{
    return std::unique_ptr<zmq::PollerWaiter>{new zmq::PollerAuto{zmq::PollerEvents::Type::Read,
        zopr_, zsnapshot_.get(), zdelivery_.get(),
        conn_->timerRetry(), conn_->timerTimeout(),
        sync_->timerTimeout()}};
}


void Worker::WorkerSession::operationReady(Operation* oper)
{
    const auto paysz = oper->payload().size();
    UNUSED(paysz);

    switch (oper->type()) {
    case Operation::Type::Start:
        LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()}, log::Arg{"started"sv});
        saveConfiguration(oper->payload());
        sendEvent(Event::Type::Started, std::move(oper->payload()));
        conn_->onStart();
        break;

    case Operation::Type::Stop:
        conn_->onStop();
        sync_->onHalt();
        sendEvent(Event::Type::Stopped, std::move(oper->payload()));
        LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()}, log::Arg{"stopped"sv});
        break;

    case Operation::Type::Dispatch:
        ++seqNum_;
        notifySequenceNumber();

        // FIXME: log::Arg shoud support int64 type, int(seqNum_) is not correct.
        LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()}, log::Arg{"dispatch"sv},
            log::Arg{"size"sv, int(paysz)}, log::Arg{"seqn"sv, int(seqNum_)});

        zdispatch_->send(std::move(
            Topic::withSeqNum(oper->payload(), seqNum_)
                .withGroup(WORKER_UPDT)));
        break;

    case Operation::Type::Sync:
        LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()}, log::Arg{"sync"sv});
        brokerUuid_ = Uuid{};
        sync_->onSync();
        break;

    default:
        LOG_ERROR(log::Arg{"worker"sv, uuid_.toShortString()},
            log::Arg{"operation"sv, Operation::toString(oper->type())},
            log::Arg{"unknown"sv});
        break;
    }
}


void Worker::WorkerSession::socketReady(zmq::Pollable* pble)
{
    if (pble == zsnapshot_.get()) {
        zmq::Part payload;
        zsnapshot_->recv(&payload);
        LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()}, log::Arg{"snapshot"sv},
            log::Arg{"size"sv, int(payload.size())});

        recvBrokerSnapshot(std::move(payload));

    } else if (pble == zdelivery_.get()) {
        zmq::Part payload;
        zdelivery_->recv(&payload);
        LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()}, log::Arg{"delivery"sv},
            log::Arg{"size"sv, int(payload.size())});

        collectBrokerMessage(std::move(payload));

    } else if (pble == conn_->timerRetry()) {
        conn_->onTimerRetryFired();

    } else if (pble == conn_->timerTimeout()) {
        conn_->onTimerTimeoutFired();

    } else if (pble == sync_->timerTimeout()) {
        sync_->onTimerTimeoutFired();

    } else {
        LOG_FATAL(log::Arg{"worker"sv, uuid_.toShortString()},
            log::Arg{"could not read ready socket"sv},
            log::Arg{"unknown socket"sv});
    }
}


void Worker::WorkerSession::connClose()
{
    // close
    zdelivery_->close();
    zdispatch_->close();
}


void Worker::WorkerSession::connOpen()
{
    // configure
    zdelivery_->setEndpoints({"ipc:///tmp/worker_delivery"});
    zdispatch_->setEndpoints({"ipc:///tmp/worker_dispatch"});

    std::list<std::string> groups{BROKER_HUGZ};
    if (!conf_.topicNames.empty()) {
        for (const auto& name : conf_.topicNames) {
            if (std::string_view(name) == BROKER_UPDT) {
                throw ERROR(Error, "could not set topic name",
                    log::Arg{"name"sv, std::string_view(BROKER_UPDT)});
            }
            groups.push_back(name);
        }
    } else {
        groups.push_back(BROKER_UPDT);
    }

    zdelivery_->setGroups(groups);

    // connect
    zdelivery_->connect();
    zdispatch_->connect();
}


void Worker::WorkerSession::snapClose()
{
    zsnapshot_->close();
}


void Worker::WorkerSession::snapOpen()
{
    zsnapshot_->setEndpoints({"ipc:///tmp/broker_snapshot"});
    zsnapshot_->connect();
}


void Worker::WorkerSession::sendAnnounce()
{
    zdispatch_->send(zmq::Part{}.withGroup(WORKER_HUGZ));
}


void Worker::WorkerSession::sendSync(uint8_t seqn)
{
    static_assert(std::is_same_v<SyncMachine::seqn_t, decltype(seqn)>);

    LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()},
        log::Arg{"snapshot"sv, "request"sv},
        log::Arg{"status"sv, Event::toString(Event::Type::SyncRequest)});

    // TODO: pass other sync params.
    zsnapshot_->trySend(zmq::PartMulti::pack(BROKER_SYNC_REQST, seqn, zmq::Part{}));
    sendEvent(Event::Type::SyncRequest, zmq::Part{});
}


void Worker::WorkerSession::saveConfiguration(const zmq::Part& part)
{
    conf_ = WorkerConfig::fromPart(part);

    subscrTopic_.clear();
    for (const auto& name : conf_.topicNames) {
        if (subscrTopic_.put(name, 0) == subscrTopic_.list().end()) {
            // restore state
            conf_ = WorkerConfig{};
            subscrTopic_.clear();

            throw ERROR(Error, "could not save configuration",
                log::Arg{"reason"sv, "too many topics"sv});
        }
    }

    seqNum_ = conf_.seqNum;
}


void Worker::WorkerSession::collectBrokerMessage(zmq::Part&& payload)
{
    const std::string_view group(payload.group());

    if (group == BROKER_HUGZ) {
        // TODO: extract the message
        conn_->onPing();

    } else if (group == BROKER_UPDT || subscrTopic_.find(group) != subscrTopic_.list().end()) {
        if (acceptTopic(payload))
            sendEvent(Event::Type::Delivery, std::move(payload));

    } else {
        LOG_WARN(log::Arg{"worker"sv, uuid_.toShortString()},
            log::Arg{"collect"sv, "recv"sv},
            log::Arg{"group"sv, std::string(payload.group())},
            log::Arg{"unknown message"sv});
    }
}


void Worker::WorkerSession::recvBrokerSnapshot(zmq::Part&& payload)
{
    auto [reply, syncseq, params] = zmq::PartMulti::unpack<std::string_view, SyncMachine::seqn_t, zmq::Part>(payload);

    if (reply == BROKER_SYNC_BEGIN) {
        brokerUuid_ = Uuid::fromPart(params);

        LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"broker", brokerUuid_.toShortString()},
            log::Arg{"status"sv, Event::toString(Event::Type::SyncBegin)});

        sendEvent(Event::Type::SyncBegin, brokerUuid_.toPart());

    } else if (reply == BROKER_SYNC_ELEMN) {
        LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"broker", brokerUuid_.toShortString()},
            log::Arg{"status"sv, Event::toString(Event::Type::SyncElement)});

        acceptTopic(params);
        sendEvent(Event::Type::SyncElement, std::move(params));
        sync_->onReply(0, syncseq, SyncMachine::ReplyType::Snapshot);

    } else if (reply == BROKER_SYNC_COMPL) {
        if (const auto uuid = Uuid::fromPart(params); uuid != brokerUuid_) {
            LOG_WARN(log::Arg{"worker"sv, uuid_.toShortString()},
                log::Arg{"snapshot"sv, "recv"sv},
                log::Arg{"old", brokerUuid_.toShortString()},
                log::Arg{"new", uuid.toShortString()},
                log::Arg{"err"sv, "broker uuid has changed"sv});

            brokerUuid_ = uuid;
        }

        sync_->onReply(0, syncseq, SyncMachine::ReplyType::Complete);

    } else {
        LOG_WARN(log::Arg{"worker"sv, uuid_.toShortString()},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"reply"sv, reply},
            log::Arg{"unknown reply"sv});
    }
}


bool Worker::WorkerSession::acceptTopic(const zmq::Part& part)
{
    // TODO: seq num and uuid might be extracted from params without constructing a full Topic.
    const auto t = Topic::fromPart(part);

    if (!acceptTopic(t.worker(), t.seqNum()))
        return false;

    if (t.worker() != conf_.uuid || t.seqNum() <= seqNum_)
        return true;

    seqNum_ = t.seqNum();
    notifySequenceNumber();

    return true;
}


bool Worker::WorkerSession::acceptTopic(const Uuid& worker, Topic::SeqN value)
{
    Topic::SeqN last{0};

    if (const auto it = workerSeqNum_.find(worker); it != workerSeqNum_.list().end())
        last = it->second;

    if (value <= last)
        return false;

    workerSeqNum_.put(worker, value);

    return true;
}


void Worker::WorkerSession::notifyConnectionUpdate(bool isUp)
{
    if (isUp == isOnline_)
        return;

    LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()},
        log::Arg{"connection"sv, isUp ? "up"sv : "down"sv});

    isOnline_ = isUp;
    sendEvent(isUp ? Event::Type::Online : Event::Type::Offline, zmq::Part{});
}


void Worker::WorkerSession::notifySnapshotDownload(bool isSync)
{
    if (isSync == isSnapshot_)
        return;

    LOG_DEBUG(log::Arg{"worker"sv, uuid_.toShortString()},
        log::Arg{"snapshot"sv, isSync ? "download"sv : "done"sv});

    isSnapshot_ = isSync;
    sendEvent(isSync ? Event::Type::SyncDownloadOn : Event::Type::SyncDownloadOff, zmq::Part{});
}


void Worker::WorkerSession::notifySequenceNumber() const
{
    try {
        if (zseqs_->trySend(zmq::Part{seqNum_}) == -1)
            throw ERROR(ZMQSocketSendFailed, "socket send would block");

    } catch (const std::exception& e) {
        LOG_FATAL(log::Arg{"runner"sv}, log::Arg{"could not notify sequence number"sv},
            log::Arg{std::string_view(e.what())});
    }
}


} // namespace fuurin
