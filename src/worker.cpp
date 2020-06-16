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
#include "fuurin/zmqtimer.h"
#include "connmachine.h"
#include "types.h"
#include "log.h"

#include <utility>
#include <chrono>
#include <cstring>


#define BROKER_HUGZ "HUGZ"
#define WORKER_HUGZ "HUGZ"
#define BROKER_UPDT "UPDT"
#define WORKER_UPDT "UPDT"


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

    sendOperation(toIntegral(Operation::Dispatch), std::forward<zmq::Part>(data));
}


std::tuple<Runner::event_type_t, zmq::Part, Runner::EventRead> Worker::waitForEvent(std::chrono::milliseconds timeout)
{
    const auto ev = Runner::waitForEvent(timeout);

    if (std::get<2>(ev) == EventRead::Success) {
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"event"sv, "recv"sv},
            log::Arg{"type"sv, eventToString(std::get<0>(ev))},
            log::Arg{"size"sv, int(std::get<1>(ev).size())});
    } else {
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"event"sv, "recv"sv},
            log::Arg{"type"sv, "n/a"sv},
            log::Arg{"size"sv, "n/a"sv});
    }

    return ev;
}


std::string_view Worker::eventToString(event_type_t ev)
{
    switch (ev) {
    case toIntegral(Runner::EventType::Invalid):
        return "invalid"sv;
    case toIntegral(Runner::EventType::Started):
        return "started"sv;
    case toIntegral(Runner::EventType::Stopped):
        return "stopped"sv;
    case toIntegral(Worker::EventType::Offline):
        return "offline"sv;
    case toIntegral(Worker::EventType::Online):
        return "online"sv;
    case toIntegral(Worker::EventType::Delivery):
        return "delivery"sv;
    default:
        return "unknown"sv;
    }
}


std::unique_ptr<Runner::Session> Worker::createSession(std::function<void()> oncompl) const
{
    return makeSession<WorkerSession>(oncompl);
}


/**
 * ASYNC TASK
 */

Worker::WorkerSession::WorkerSession(token_type_t token, std::function<void()> oncompl,
    const std::unique_ptr<zmq::Context>& zctx,
    const std::unique_ptr<zmq::Socket>& zoper,
    const std::unique_ptr<zmq::Socket>& zevent)
    : Session(token, oncompl, zctx, zoper, zevent)
    , zsnapshot_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::CLIENT)}
    , zdelivery_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::DISH)}
    , zdispatch_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::RADIO)}
    , conn_{
          std::make_unique<ConnMachine>("worker"sv, zctx.get(),
              500ms,  // TODO: make configurable
              3000ms, // TODO: make configurable
              std::bind(&Worker::WorkerSession::connClose, this),
              std::bind(&Worker::WorkerSession::connOpen, this),
              std::bind(&Worker::WorkerSession::sendAnnounce, this),
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
    , isOnline_{false}
{
    zsnapshot_->setEndpoints({"ipc:///tmp/broker_snapshot"});
    zsnapshot_->connect();
}


Worker::WorkerSession::~WorkerSession() noexcept = default;


bool Worker::WorkerSession::isOnline() const noexcept
{
    return isOnline_;
}


std::unique_ptr<zmq::PollerWaiter> Worker::WorkerSession::createPoller()
{
    return std::unique_ptr<zmq::PollerWaiter>{new zmq::PollerAuto{zmq::PollerEvents::Type::Read,
        zopr_.get(), zsnapshot_.get(), zdelivery_.get(), conn_->timerRetry(), conn_->timerTimeout()}};
}


void Worker::WorkerSession::operationReady(oper_type_t oper, zmq::Part&& payload)
{
    const auto paysz = payload.size();
    UNUSED(paysz);

    switch (oper) {
    case toIntegral(Runner::Operation::Start):
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"started"sv});
        sendEvent(toIntegral(Runner::EventType::Started), std::move(payload));
        conn_->onStart();
        break;

    case toIntegral(Runner::Operation::Stop):
        conn_->onStop();
        sendEvent(toIntegral(Runner::EventType::Stopped), std::move(payload));
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"stopped"sv});
        break;

    case toIntegral(Worker::Operation::Dispatch):
        zdispatch_->send(payload.withGroup(WORKER_UPDT));
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"dispatch"sv},
            log::Arg{"size"sv, int(paysz)});
        break;

    default:
        LOG_ERROR(log::Arg{"worker"sv}, log::Arg{"operation"sv, oper},
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

        // TODO: use snapshot payload

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


void Worker::WorkerSession::sendAnnounce()
{
    zdispatch_->send(zmq::Part{}.withGroup(WORKER_HUGZ));
}


void Worker::WorkerSession::collectBrokerMessage(zmq::Part&& payload)
{
    if (std::strncmp(payload.group(), BROKER_HUGZ, sizeof(BROKER_HUGZ)) == 0) {
        // TODO: extract the message
        conn_->onPing();

    } else if (std::strncmp(payload.group(), BROKER_UPDT, sizeof(BROKER_UPDT)) == 0) {
        sendEvent(toIntegral(EventType::Delivery), std::move(payload));

    } else {
        LOG_WARN(log::Arg{"worker"sv},
            log::Arg{"collect"sv, "recv"sv},
            log::Arg{"group"sv, std::string(payload.group())},
            log::Arg{"unknown message"sv});
    }
}


void Worker::WorkerSession::notifyConnectionUpdate(bool isUp)
{
    if (isUp == isOnline_)
        return;

    LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"connection"sv, isUp ? "up"sv : "down"sv});

    isOnline_ = isUp;
    sendEvent(toIntegral(isUp ? EventType::Online : EventType::Offline), zmq::Part{});
}

} // namespace fuurin
