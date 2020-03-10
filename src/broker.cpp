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

/**
 * MAIN TASK
 */

Broker::Broker()
{
}


Broker::~Broker() noexcept
{
}


std::unique_ptr<Runner::Session> Broker::makeSession(std::function<void()> oncompl) const
{
    return std::make_unique<BrokerSession>(token_, oncompl, zctx_, zopr_, zevs_);
}


/**
 * ASYNC TASK
 */

Broker::BrokerSession::BrokerSession(token_type_t token, std::function<void()> oncompl,
    const std::unique_ptr<zmq::Context>& zctx,
    const std::unique_ptr<zmq::Socket>& zoper,
    const std::unique_ptr<zmq::Socket>& zevent)
    : Session(token, oncompl, zctx, zoper, zevent)
    , zstore_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::SERVER)}
{
    zstore_->setEndpoints({"ipc:///tmp/broker_storage"});
    zstore_->bind();
}


Broker::BrokerSession::~BrokerSession() noexcept = default;


std::unique_ptr<zmq::PollerWaiter> Broker::BrokerSession::createPoller()
{
    auto poll = new zmq::Poller{zmq::PollerEvents::Type::Read, zopr_.get(), zstore_.get()};
    return std::unique_ptr<zmq::PollerWaiter>{poll};
}


void Broker::BrokerSession::socketReady(zmq::Pollable* pble)
{
    zmq::Part payload;

    if (pble == zstore_.get()) {
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
