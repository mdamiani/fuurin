/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/broker.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "log.h"


namespace fuurin {


Broker::Broker()
{
}


Broker::~Broker() noexcept
{
}

std::unique_ptr<zmq::PollerWaiter> Broker::createPoller(zmq::Socket* sock)
{
    zstore_ = std::make_unique<zmq::Socket>(zmqContext(), zmq::Socket::SERVER);
    zstore_->setEndpoints({"ipc:///tmp/broker_storage"});
    zstore_->bind();

    auto poll = new zmq::PollerAuto(zmq::PollerEvents::Type::Read, sock, zstore_.get());
    return std::unique_ptr<zmq::PollerWaiter>(poll);
}


void Broker::socketReady(zmq::Socket* sock)
{
    zmq::Part payload;

    if (zstore_.get() == sock) {
        zstore_->recv(&payload);
        LOG_DEBUG(log::Arg{"broker"sv}, log::Arg{"store"sv, "recv"sv},
            log::Arg{"size"sv, int(payload.size())});
    } else {
        LOG_FATAL(log::Arg{"broker"sv}, log::Arg{"could not read ready socket"sv},
            log::Arg{"unknown socket"sv});
    }

    zstore_->send(payload);
}


} // namespace fuurin
