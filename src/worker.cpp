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
#include "log.h"

#include <utility>
#include <string_view>


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


void Worker::store(zmq::Part&& data)
{
    if (!isRunning()) {
        throw ERROR(Error, "could not store data",
            log::Arg{"reason"sv, "worker is not running"sv});
    }

    sendOperation(Operation::Store, std::forward<zmq::Part>(data));
}

/**
 * ASYNC TASK
 */

std::unique_ptr<zmq::PollerWaiter> Worker::createPoller(zmq::Socket* sock)
{
    zstore_ = std::make_unique<zmq::Socket>(zmqContext(), zmq::Socket::CLIENT);
    zstore_->setEndpoints({"ipc:///tmp/broker_storage"});
    zstore_->connect();

    auto poll = new zmq::PollerAuto{zmq::PollerEvents::Type::Read, sock, zstore_.get()};
    return std::unique_ptr<zmq::PollerWaiter>{poll};
}


void Worker::operationReady(oper_type_t oper, zmq::Part& payload)
{
#ifndef NDEBUG
    const auto paysz = payload.size();
#endif

    switch (oper) {
    case Worker::Operation::Store:
        zstore_->send(payload);
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"store"sv, "send"sv},
            log::Arg{"size"sv, int(paysz)});
        break;
    default:
        break;
    }
}

void Worker::socketReady(zmq::Socket* sock)
{
    zmq::Part payload;

    if (zstore_.get() == sock) {
        zstore_->recv(&payload);
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"store"sv, "recv"sv},
            log::Arg{"size"sv, int(payload.size())});
    } else {
        LOG_FATAL(log::Arg{"worker"sv}, log::Arg{"could not read ready socket"sv},
            log::Arg{"unknown socket"sv});
    }

    sendEvent(std::move(payload));
}


std::optional<zmq::Part> Worker::waitForEvent(std::chrono::milliseconds timeout)
{
    const auto ev = Runner::waitForEvent(timeout);

    if (ev.has_value()) {
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"event"sv, "recv"sv},
            log::Arg{"size"sv, int(ev.value().size())});
    } else {
        LOG_DEBUG(log::Arg{"worker"sv}, log::Arg{"event"sv, "recv"sv},
            log::Arg{"size"sv, "n/a"sv});
    }

    return ev;
}

} // namespace fuurin
