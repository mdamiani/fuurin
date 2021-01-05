/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/sessionworker.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"
#include "fuurin/zmqtimer.h"
#include "fuurin/errors.h"
#include "connmachine.h"
#include "syncmachine.h"
#include "types.h"
#include "log.h"

#include <algorithm>
#include <utility>
#include <list>
#include <set>
#include <type_traits>
#include <string_view>


using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;


namespace fuurin {


WorkerSession::WorkerSession(const std::string& name, Uuid id, SessionEnv::token_t token,
    zmq::Context* zctx, zmq::Socket* zfin, zmq::Socket* zoper, zmq::Socket* zevent,
    zmq::Socket* zseqs)
    : Session(name, id, token, zctx, zfin, zoper, zevent)
    , zsnapshot_{std::make_unique<zmq::Socket>(zctx, zmq::Socket::CLIENT)}
    , zdelivery_{std::make_unique<zmq::Socket>(zctx, zmq::Socket::DISH)}
    , zdispatch_{std::make_unique<zmq::Socket>(zctx, zmq::Socket::RADIO)}
    , conn_{
          std::make_unique<ConnMachine>(name_, id, zctx,
              500ms,  // TODO: make configurable
              3000ms, // TODO: make configurable

              std::bind(&WorkerSession::connClose, this),    //
              std::bind(&WorkerSession::connOpen, this),     //
              std::bind(&WorkerSession::sendAnnounce, this), //
              [this](ConnMachine::State s) {
                  onConnChanged(std::underlying_type_t<ConnMachine::State>(s));
              }),
      }
    , sync_{
          std::make_unique<SyncMachine>(name_, id, zctx,
              0,      // TODO: depends on number of endpoints
              1,      // TODO: make configurable
              3000ms, // TODO: make configurable

              std::bind(&WorkerSession::snapClose, this),                       //
              std::bind(&WorkerSession::snapOpen, this),                        //
              std::bind(&WorkerSession::sendSync, this, std::placeholders::_2), //
              [this](SyncMachine::State s) {
                  onSyncChanged(std::underlying_type_t<SyncMachine::State>(s));
              }),
      }
    , zseqs_{zseqs}
    , isOnline_{false}
    , isSnapshot_{false}
{
}


WorkerSession::~WorkerSession() noexcept = default;


bool WorkerSession::isOnline() const noexcept
{
    return isOnline_;
}


std::unique_ptr<zmq::PollerWaiter> WorkerSession::createPoller()
{
    return std::unique_ptr<zmq::PollerWaiter>{new zmq::PollerAuto{zmq::PollerEvents::Type::Read,
        zopr_, zsnapshot_.get(), zdelivery_.get(),
        conn_->timerRetry(), conn_->timerTimeout(),
        sync_->timerTimeout()}};
}


void WorkerSession::operationReady(Operation* oper)
{
    const auto paysz = oper->payload().size();
    UNUSED(paysz);

    switch (oper->type()) {
    case Operation::Type::Start:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"started"sv});
        saveConfiguration(oper->payload());
        sendEvent(Event::Type::Started, std::move(oper->payload()));
        conn_->onStart();
        break;

    case Operation::Type::Stop:
        conn_->onStop();
        sync_->onHalt();
        sendEvent(Event::Type::Stopped, std::move(oper->payload()));
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"stopped"sv});
        break;

    case Operation::Type::Dispatch:
        ++seqNum_;
        notifySequenceNumber();

        // FIXME: log::Arg shoud support int64 type, int(seqNum_) is not correct.
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"dispatch"sv},
            log::Arg{"size"sv, int(paysz)}, log::Arg{"seqn"sv, int(seqNum_)});

        zdispatch_->send(std::move(
            Topic::withSeqNum(oper->payload(), seqNum_)
                .withGroup(SessionEnv::WorkerUpdt.data())));
        break;

    case Operation::Type::Sync:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"sync"sv});
        brokerUuid_ = Uuid{};
        sync_->onSync();
        break;

    default:
        LOG_ERROR(log::Arg{name_, uuid_.toShortString()},
            log::Arg{"operation"sv, Operation::toString(oper->type())},
            log::Arg{"unknown"sv});
        break;
    }
}


void WorkerSession::socketReady(zmq::Pollable* pble)
{
    if (pble == zsnapshot_.get()) {
        zmq::Part payload;
        zsnapshot_->recv(&payload);
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"snapshot"sv},
            log::Arg{"size"sv, int(payload.size())});

        recvBrokerSnapshot(std::move(payload));

    } else if (pble == zdelivery_.get()) {
        zmq::Part payload;
        zdelivery_->recv(&payload);
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"delivery"sv},
            log::Arg{"size"sv, int(payload.size())});

        collectBrokerMessage(std::move(payload));

    } else if (pble == conn_->timerRetry()) {
        conn_->onTimerRetryFired();

    } else if (pble == conn_->timerTimeout()) {
        conn_->onTimerTimeoutFired();

    } else if (pble == sync_->timerTimeout()) {
        sync_->onTimerTimeoutFired();

    } else {
        LOG_FATAL(log::Arg{name_, uuid_.toShortString()},
            log::Arg{"could not read ready socket"sv},
            log::Arg{"unknown socket"sv});
    }
}


void WorkerSession::connClose()
{
    // close
    zdelivery_->close();
    zdispatch_->close();
}


void WorkerSession::connOpen()
{
    // configure
    zdelivery_->setEndpoints({conf_.endpDelivery.begin(), conf_.endpDelivery.end()});
    zdispatch_->setEndpoints({conf_.endpDispatch.begin(), conf_.endpDispatch.end()});

    std::set<std::string> groups{{SessionEnv::BrokerHugz.data(), SessionEnv::BrokerHugz.size()}};

    if (!conf_.topicsAll) {
        for (const auto& name : conf_.topicsNames) {
            if (std::string_view(name) == SessionEnv::BrokerUpdt) {
                throw ERROR(Error, "could not set topic name",
                    log::Arg{"name"sv, SessionEnv::BrokerUpdt});
            }
            groups.insert(name);
        }
    } else {
        groups.insert(SessionEnv::BrokerUpdt.data());
    }

    zdelivery_->setGroups({groups.begin(), groups.end()});

    // connect
    zdelivery_->connect();
    zdispatch_->connect();
}


void WorkerSession::snapClose()
{
    zsnapshot_->close();
}


void WorkerSession::snapOpen()
{
    zsnapshot_->setEndpoints({conf_.endpSnapshot.begin(), conf_.endpSnapshot.end()});
    zsnapshot_->connect();
}


void WorkerSession::sendAnnounce()
{
    zdispatch_->send(zmq::Part{}.withGroup(SessionEnv::WorkerHugz.data()));
}


void WorkerSession::sendSync(uint8_t syncseq)
{
    static_assert(std::is_same_v<SyncMachine::seqn_t, decltype(syncseq)>);

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()},
        log::Arg{"snapshot"sv, "request"sv},
        log::Arg{"status"sv, Event::toString(Event::Type::SyncRequest)});

    auto conf = conf_;
    conf.seqNum = seqNum_;
    auto params = conf.toPart();

    zsnapshot_->trySend(zmq::PartMulti::pack(SessionEnv::BrokerSyncReqst, syncseq, zmq::Part{params}));
    sendEvent(Event::Type::SyncRequest, std::move(params));
}


void WorkerSession::saveConfiguration(const zmq::Part& part)
{
    conf_ = WorkerConfig::fromPart(part);

    subscrTopic_.clear();
    for (const auto& name : conf_.topicsNames) {
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


void WorkerSession::collectBrokerMessage(zmq::Part&& payload)
{
    const std::string_view group(payload.group());

    if (group == SessionEnv::BrokerHugz) {
        // TODO: check whether payload corresponds to the dispatched probe.
        //       in this case we are sure the full round trip works.
        conn_->onPing();

    } else if (group == SessionEnv::BrokerUpdt || subscrTopic_.find(group) != subscrTopic_.list().end()) {
        if (acceptTopic(payload))
            sendEvent(Event::Type::Delivery, std::move(payload));

    } else {
        LOG_WARN(log::Arg{name_, uuid_.toShortString()},
            log::Arg{"collect"sv, "recv"sv},
            log::Arg{"group"sv, std::string(payload.group())},
            log::Arg{"unknown message"sv});
    }
}


void WorkerSession::recvBrokerSnapshot(zmq::Part&& payload)
{
    auto [reply, syncseq, params] = zmq::PartMulti::unpack<std::string_view, SyncMachine::seqn_t, zmq::Part>(payload);

    if (reply == SessionEnv::BrokerSyncBegin) {
        brokerUuid_ = Uuid::fromPart(params);

        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"broker", brokerUuid_.toShortString()},
            log::Arg{"status"sv, Event::toString(Event::Type::SyncBegin)});

        sendEvent(Event::Type::SyncBegin, brokerUuid_.toPart());

    } else if (reply == SessionEnv::BrokerSyncElemn) {
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"broker", brokerUuid_.toShortString()},
            log::Arg{"status"sv, Event::toString(Event::Type::SyncElement)});

        acceptTopic(params);
        sendEvent(Event::Type::SyncElement, std::move(params));
        sync_->onReply(0, syncseq, SyncMachine::ReplyType::Snapshot);

    } else if (reply == SessionEnv::BrokerSyncCompl) {
        if (const auto uuid = Uuid::fromPart(params); uuid != brokerUuid_) {
            LOG_WARN(log::Arg{name_, uuid_.toShortString()},
                log::Arg{"snapshot"sv, "recv"sv},
                log::Arg{"old", brokerUuid_.toShortString()},
                log::Arg{"new", uuid.toShortString()},
                log::Arg{"err"sv, "broker uuid has changed"sv});

            brokerUuid_ = uuid;
        }

        sync_->onReply(0, syncseq, SyncMachine::ReplyType::Complete);

    } else {
        LOG_WARN(log::Arg{name_, uuid_.toShortString()},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"reply"sv, reply},
            log::Arg{"unknown reply"sv});
    }
}


bool WorkerSession::acceptTopic(const zmq::Part& part)
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


bool WorkerSession::acceptTopic(const Uuid& worker, Topic::SeqN value)
{
    Topic::SeqN last{0};

    if (const auto it = workerSeqNum_.find(worker); it != workerSeqNum_.list().end())
        last = it->second;

    if (value <= last)
        return false;

    workerSeqNum_.put(worker, value);

    return true;
}


void WorkerSession::onConnChanged(int newState)
{
    static_assert(std::is_same_v<decltype(newState), std::underlying_type_t<ConnMachine::State>>,
        "invalid type of parameter");

    switch (ConnMachine::State(newState)) {
    case ConnMachine::State::Halted:
    case ConnMachine::State::Trying:
        notifyConnectionUpdate(false);
        break;

    case ConnMachine::State::Stable:
        notifyConnectionUpdate(true);
        break;
    };
}


void WorkerSession::onSyncChanged(int newState)
{
    static_assert(std::is_same_v<decltype(newState), std::underlying_type_t<SyncMachine::State>>,
        "invalid type of parameter");

    switch (SyncMachine::State(newState)) {
    case SyncMachine::State::Halted:
        if (isSnapshot_) {
            LOG_DEBUG(log::Arg{name_, uuid_.toShortString()},
                log::Arg{"snapshot"sv, "halt"sv},
                log::Arg{"broker", brokerUuid_.toShortString()},
                log::Arg{"status"sv, Event::toString(Event::Type::SyncError)});

            sendEvent(Event::Type::SyncError, brokerUuid_.toPart());
        }
        brokerUuid_ = Uuid{};
        notifySnapshotDownload(false);
        break;

    case SyncMachine::State::Synced:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"broker", brokerUuid_.toShortString()},
            log::Arg{"status"sv, Event::toString(Event::Type::SyncSuccess)});

        sendEvent(Event::Type::SyncSuccess, brokerUuid_.toPart());
        notifySnapshotDownload(false);
        break;

    case SyncMachine::State::Failed:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()},
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
}


void WorkerSession::notifyConnectionUpdate(bool isUp)
{
    if (isUp == isOnline_)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()},
        log::Arg{"connection"sv, isUp ? "up"sv : "down"sv});

    isOnline_ = isUp;
    sendEvent(isUp ? Event::Type::Online : Event::Type::Offline, zmq::Part{});
}


void WorkerSession::notifySnapshotDownload(bool isSync)
{
    if (isSync == isSnapshot_)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()},
        log::Arg{"snapshot"sv, isSync ? "download"sv : "done"sv});

    isSnapshot_ = isSync;
    sendEvent(isSync ? Event::Type::SyncDownloadOn : Event::Type::SyncDownloadOff, zmq::Part{});
}


void WorkerSession::notifySequenceNumber() const
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
