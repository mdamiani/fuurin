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

#include <utility>
#include <chrono>
#include <cstring>
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

Worker::Worker()
{
}


Worker::~Worker() noexcept
{
}


void Worker::dispatch(zmq::Part&& data)
{
    if (!isRunning()) {
        throw ERROR(Error, "could not dispatch data",
            log::Arg{"reason"sv, "worker is not running"sv});
    }

    sendOperation(Operation::Type::Dispatch, std::forward<zmq::Part>(data));
}


void Worker::sync()
{
    if (!isRunning()) {
        throw ERROR(Error, "could not sync",
            log::Arg{"reason"sv, "worker is not running"sv});
    }

    sendOperation(Operation::Type::Sync);
}


Event Worker::waitForEvent(std::chrono::milliseconds timeout)
{
    const auto& ev = Runner::waitForEvent(timeout);

    if (ev.notification() == Event::Notification::Success) {
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"event"sv, "recv"sv},
            log::Arg{"type"sv, Event::toString(ev.type())},
            log::Arg{"size"sv, int(ev.payload().size())});
    } else {
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"event"sv, "recv"sv},
            log::Arg{"type"sv, "n/a"sv},
            log::Arg{"size"sv, "n/a"sv});
    }

    return ev;
}


std::unique_ptr<Runner::Session> Worker::createSession(CompletionFunc onComplete) const
{
    return makeSession<WorkerSession>(onComplete);
}


/**
 * ASYNC TASK
 */

Worker::WorkerSession::WorkerSession(token_type_t token, CompletionFunc onComplete,
    const std::unique_ptr<zmq::Context>& zctx,
    const std::unique_ptr<zmq::Socket>& zoper,
    const std::unique_ptr<zmq::Socket>& zevent)
    : Session(token, onComplete, zctx, zoper, zevent)
    , zsnapshot_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::CLIENT)}
    , zdelivery_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::DISH)}
    , zdispatch_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::RADIO)}
    , conn_{
          std::make_unique<ConnMachine>("worker"sv, zctx.get(),
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
          std::make_unique<SyncMachine>("worker"sv, zctx.get(),
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
                          LOG_DEBUG(log::Arg{"worker"sv},
                              log::Arg{"snapshot"sv, "halt"sv},
                              log::Arg{"status"sv, Event::toString(Event::Type::SyncError)});

                          sendEvent(Event::Type::SyncError, zmq::Part{});
                      }
                      notifySnapshotDownload(false);
                      break;

                  case SyncMachine::State::Synced:
                      LOG_DEBUG(log::Arg{"worker"sv},
                          log::Arg{"snapshot"sv, "recv"sv},
                          log::Arg{"status"sv, Event::toString(Event::Type::SyncSuccess)});

                      sendEvent(Event::Type::SyncSuccess, zmq::Part{});
                      notifySnapshotDownload(false);
                      break;

                  case SyncMachine::State::Failed:
                      LOG_DEBUG(log::Arg{"worker"sv},
                          log::Arg{"snapshot"sv, "timeout"sv},
                          log::Arg{"status"sv, Event::toString(Event::Type::SyncError)});

                      sendEvent(Event::Type::SyncError, zmq::Part{});
                      notifySnapshotDownload(false);
                      break;

                  case SyncMachine::State::Download:
                      notifySnapshotDownload(true);
                      break;
                  };
              }),
      }
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
        zopr_.get(), zsnapshot_.get(), zdelivery_.get(),
        conn_->timerRetry(), conn_->timerTimeout(),
        sync_->timerTimeout()}};
}


void Worker::WorkerSession::operationReady(Operation* oper)
{
    const auto paysz = oper->payload().size();
    UNUSED(paysz);

    switch (oper->type()) {
    case Operation::Type::Start:
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"started"sv});
        sendEvent(Event::Type::Started, std::move(oper->payload()));
        conn_->onStart();
        break;

    case Operation::Type::Stop:
        conn_->onStop();
        sync_->onHalt();
        sendEvent(Event::Type::Stopped, std::move(oper->payload()));
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"stopped"sv});
        break;

    case Operation::Type::Dispatch:
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"dispatch"sv},
            log::Arg{"size"sv, int(paysz)});
        zdispatch_->send(oper->payload().withGroup(WORKER_UPDT));
        break;

    case Operation::Type::Sync:
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"sync"sv});
        sync_->onSync();
        break;

    default:
        LOG_ERROR(log::Arg{"worker"sv}, log::Arg{"operation"sv, Operation::toString(oper->type())},
            log::Arg{"unknown"sv});
        break;
    }
}


void Worker::WorkerSession::socketReady(zmq::Pollable* pble)
{
    if (pble == zsnapshot_.get()) {
        zmq::Part payload;
        zsnapshot_->recv(&payload);
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"snapshot"sv},
            log::Arg{"size"sv, int(payload.size())});

        recvBrokerSnapshot(std::move(payload));

    } else if (pble == zdelivery_.get()) {
        zmq::Part payload;
        zdelivery_->recv(&payload);
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"delivery"sv},
            log::Arg{"size"sv, int(payload.size())});

        collectBrokerMessage(std::move(payload));

    } else if (pble == conn_->timerRetry()) {
        conn_->onTimerRetryFired();

    } else if (pble == conn_->timerTimeout()) {
        conn_->onTimerTimeoutFired();

    } else if (pble == sync_->timerTimeout()) {
        sync_->onTimerTimeoutFired();

    } else {
        LOG_FATAL(log::Arg{"worker"sv}, log::Arg{"could not read ready socket"sv},
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

    zdelivery_->setGroups({BROKER_HUGZ, BROKER_UPDT});

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

    LOG_DEBUG(log::Arg{"worker"sv},
        log::Arg{"snapshot"sv, "request"sv},
        log::Arg{"status"sv, Event::toString(Event::Type::SyncBegin)});

    // TODO: pass other sync params.
    zsnapshot_->trySend(zmq::PartMulti::pack(BROKER_SYNC_REQST, seqn, zmq::Part{}));
    sendEvent(Event::Type::SyncBegin, zmq::Part{});
}


void Worker::WorkerSession::collectBrokerMessage(zmq::Part&& payload)
{
    if (std::strncmp(payload.group(), BROKER_HUGZ, sizeof(BROKER_HUGZ)) == 0) {
        // TODO: extract the message
        conn_->onPing();

    } else if (std::strncmp(payload.group(), BROKER_UPDT, sizeof(BROKER_UPDT)) == 0) {
        sendEvent(Event::Type::Delivery, std::move(payload));

    } else {
        LOG_WARN(log::Arg{"worker"sv},
            log::Arg{"collect"sv, "recv"sv},
            log::Arg{"group"sv, std::string(payload.group())},
            log::Arg{"unknown message"sv});
    }
}


void Worker::WorkerSession::recvBrokerSnapshot(zmq::Part&& payload)
{
    auto [reply, seqn, params] = zmq::PartMulti::unpack<std::string_view, SyncMachine::seqn_t, zmq::Part>(payload);

    if (reply == BROKER_SYNC_BEGIN) {
        LOG_DEBUG(log::Arg{"worker"sv},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"status"sv, Event::toString(Event::Type::SyncBegin)});

    } else if (reply == BROKER_SYNC_ELEMN) {
        LOG_DEBUG(log::Arg{"worker"sv},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"status"sv, Event::toString(Event::Type::SyncElement)});

        sendEvent(Event::Type::SyncElement, std::move(params));
        sync_->onReply(0, seqn, SyncMachine::ReplyType::Snapshot);

    } else if (reply == BROKER_SYNC_COMPL) {
        sync_->onReply(0, seqn, SyncMachine::ReplyType::Complete);

    } else {
        LOG_WARN(log::Arg{"worker"sv},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"reply"sv, reply},
            log::Arg{"unknown reply"sv});
    }
}


void Worker::WorkerSession::notifyConnectionUpdate(bool isUp)
{
    if (isUp == isOnline_)
        return;

    LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"connection"sv, isUp ? "up"sv : "down"sv});

    isOnline_ = isUp;
    sendEvent(isUp ? Event::Type::Online : Event::Type::Offline, zmq::Part{});
}


void Worker::WorkerSession::notifySnapshotDownload(bool isSync)
{
    if (isSync == isSnapshot_)
        return;

    LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"snapshot"sv, isSync ? "download"sv : "done"sv});

    isSnapshot_ = isSync;
    sendEvent(isSync ? Event::Type::SyncDownloadOn : Event::Type::SyncDownloadOff, zmq::Part{});
}

} // namespace fuurin