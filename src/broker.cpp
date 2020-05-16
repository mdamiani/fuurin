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
#include "fuurin/zmqtimer.h"
#include "log.h"


#define BROKER_HUGZ "HUGZ"
#define WORKER_UPDT "UPDT"


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
    , zcollect_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::DISH)}
    , zdeliver_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::RADIO)}
    , zhugz_{std::make_unique<zmq::Timer>(zctx.get(), "hugz")}
{
    zstore_->setEndpoints({"ipc:///tmp/broker_storage"});
    zcollect_->setEndpoints({"ipc:///tmp/worker_deliver"});
    zdeliver_->setEndpoints({"ipc:///tmp/worker_collect"});

    zcollect_->setGroups({WORKER_UPDT});

    zstore_->bind();
    zcollect_->bind();
    zdeliver_->bind();

    zhugz_->setInterval(1s);
    zhugz_->setSingleShot(false);
}


Broker::BrokerSession::~BrokerSession() noexcept = default;


std::unique_ptr<zmq::PollerWaiter> Broker::BrokerSession::createPoller()
{
    return std::unique_ptr<zmq::PollerWaiter>{new zmq::Poller{zmq::PollerEvents::Type::Read,
        zopr_.get(), zstore_.get(), zhugz_.get()}};
}


void Broker::BrokerSession::socketReady(zmq::Pollable* pble)
{
    if (pble == zstore_.get()) {
        zmq::Part payload;
        zstore_->recv(&payload);
        LOG_DEBUG(log::Arg{"broker"sv}, log::Arg{"store"sv, "recv"sv},
            log::Arg{"size"sv, int(payload.size())});

        zstore_->send(payload);

    } else if (pble == zcollect_.get()) {
        zmq::Part payload;
        zcollect_->recv(&payload);
        LOG_DEBUG(log::Arg{"broker"sv}, log::Arg{"collect"sv, "recv"sv},
            log::Arg{"size"sv, int(payload.size())});

    } else if (pble == zhugz_.get()) {
        zhugz_->consume();
        sendHugz();

    } else {
        LOG_FATAL(log::Arg{"broker"sv}, log::Arg{"could not read ready socket"sv},
            log::Arg{"unknown socket"sv});
    }
}


void Broker::BrokerSession::collectWorkerMessage(zmq::Part&& payload)
{
    if (payload.group() == WORKER_UPDT) {
        // TODO: extract the message
        if (!zhugz_->isActive())
            zhugz_->start();

    } else {
        LOG_WARN(log::Arg{"broker"sv},
            log::Arg{"collect"sv, "recv"sv},
            log::Arg{"group"sv, std::string(payload.group())},
            log::Arg{"unknown message"sv});
    }
}


void Broker::BrokerSession::sendHugz()
{
    // TODO: send network status update.
    zdeliver_->send(zmq::Part{}.withGroup(BROKER_HUGZ));
}

} // namespace fuurin
