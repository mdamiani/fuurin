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
#include "fuurin/zmqpartmulti.h"
#include "fuurin/zmqtimer.h"
#include "syncmachine.h"
#include "types.h"
#include "log.h"

#include <zmq.h>

#include <cstring>
#include <string_view>
#include <type_traits>


#define BROKER_HUGZ "HUGZ"
#define WORKER_HUGZ "HUGZ"
#define BROKER_UPDT "UPDT"
#define WORKER_UPDT "UPDT"

#define BROKER_SYNC_REQST "SYNC"sv
#define BROKER_SYNC_BEGIN "BEGN"sv
#define BROKER_SYNC_ELEMN "ELEM"sv
#define BROKER_SYNC_COMPL "SONC"sv


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


std::unique_ptr<Runner::Session> Broker::createSession(CompletionFunc onComplete) const
{
    return makeSession<BrokerSession>(onComplete);
}


/**
 * ASYNC TASK
 */

Broker::BrokerSession::BrokerSession(token_type_t token, CompletionFunc onComplete,
    const std::unique_ptr<zmq::Context>& zctx,
    const std::unique_ptr<zmq::Socket>& zoper,
    const std::unique_ptr<zmq::Socket>& zevent)
    : Session(token, onComplete, zctx, zoper, zevent)
    , zsnapshot_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::SERVER)}
    , zdelivery_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::DISH)}
    , zdispatch_{std::make_unique<zmq::Socket>(zctx.get(), zmq::Socket::RADIO)}
    , zhugz_{std::make_unique<zmq::Timer>(zctx.get(), "hugz")}
{
    zsnapshot_->setEndpoints({"ipc:///tmp/broker_snapshot"});
    zdelivery_->setEndpoints({"ipc:///tmp/worker_dispatch"});
    zdispatch_->setEndpoints({"ipc:///tmp/worker_delivery"});

    zdelivery_->setGroups({WORKER_HUGZ, WORKER_UPDT});

    zsnapshot_->bind();
    zdelivery_->bind();
    zdispatch_->bind();

    zhugz_->setInterval(1s);
    zhugz_->setSingleShot(false);
}


Broker::BrokerSession::~BrokerSession() noexcept = default;


std::unique_ptr<zmq::PollerWaiter> Broker::BrokerSession::createPoller()
{
    return std::unique_ptr<zmq::PollerWaiter>{new zmq::Poller{zmq::PollerEvents::Type::Read,
        zopr_.get(), zsnapshot_.get(), zdelivery_.get(), zhugz_.get()}};
}


void Broker::BrokerSession::operationReady(Operation* oper)
{
    switch (oper->type()) {
    case Operation::Type::Start:
        LOG_DEBUG(log::Arg{"broker"sv}, log::Arg{"started"sv});
        break;

    case Operation::Type::Stop:
        LOG_DEBUG(log::Arg{"broker"sv}, log::Arg{"stopped"sv});
        break;

    default:
        LOG_ERROR(log::Arg{"broker"sv}, log::Arg{"operation"sv, Operation::toString(oper->type())},
            log::Arg{"unknown"sv});
        break;
    }
}


void Broker::BrokerSession::socketReady(zmq::Pollable* pble)
{
    if (pble == zsnapshot_.get()) {
        zmq::Part payload;
        zsnapshot_->recv(&payload);
        LOG_DEBUG(log::Arg{"broker"sv}, log::Arg{"snapshot"sv},
            log::Arg{"size"sv, int(payload.size())});

        receiveWorkerCommand(std::move(payload));

    } else if (pble == zdelivery_.get()) {
        zmq::Part payload;
        zdelivery_->recv(&payload);
        LOG_DEBUG(log::Arg{"broker"sv}, log::Arg{"delivery"sv},
            log::Arg{"size"sv, int(payload.size())});

        collectWorkerMessage(std::move(payload));

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
    const auto paysz = payload.size();
    UNUSED(paysz);

    if (std::strncmp(payload.group(), WORKER_HUGZ, sizeof(WORKER_HUGZ)) == 0) {
        // TODO: extract the message
        if (!zhugz_->isActive())
            zhugz_->start();

    } else if (std::strncmp(payload.group(), WORKER_UPDT, sizeof(WORKER_UPDT)) == 0) {
        LOG_DEBUG(log::Arg{"broker"sv}, log::Arg{"dispatch"sv},
            log::Arg{"size"sv, int(paysz)});

        // TODO: set broker id.
        const auto t = Topic::fromPart(payload).withBroker(Uuid{});

        storage_.push_back(t);

        zdispatch_->send(t.toPart().withGroup(BROKER_UPDT));

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
    zdispatch_->send(zmq::Part{}.withGroup(BROKER_HUGZ));
}


void Broker::BrokerSession::receiveWorkerCommand(zmq::Part&& payload)
{
    // TODO: use snapshot payload
    auto [req, seqn, params] = zmq::PartMulti::unpack<std::string_view, SyncMachine::seqn_t, zmq::Part>(payload);

    if (req != BROKER_SYNC_REQST) {
        LOG_WARN(log::Arg{"broker"sv},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"request"sv, req},
            log::Arg{"seqn"sv, seqn},
            log::Arg{"unknown request"sv});

        return;
    }

    replySnapshot(payload.routingID(), seqn, std::move(params));
}


void Broker::BrokerSession::replySnapshot(uint32_t rouID, uint8_t seqn, zmq::Part&&)
{
    static_assert(std::is_same_v<SyncMachine::seqn_t, decltype(seqn)>);

    LOG_DEBUG(log::Arg{"broker"sv}, log::Arg{"sync"sv, "reply"sv},
        log::Arg{"elements"sv, int(storage_.size())});

    try {
        const auto& errWouldBlock = ERROR(ZMQSocketSendFailed, "",
            log::Arg{log::ec_t{EAGAIN}});

        if (zsnapshot_->trySend(zmq::PartMulti::pack(BROKER_SYNC_BEGIN, seqn, ""sv)
                                    .withRoutingID(rouID)) == -1) {
            throw errWouldBlock;
        }
        for (const auto& t : storage_) {
            if (zsnapshot_->trySend(zmq::PartMulti::pack(BROKER_SYNC_ELEMN, seqn, t.toPart())
                                        .withRoutingID(rouID)) == -1) {
                throw errWouldBlock;
            }
        }
        if (zsnapshot_->trySend(zmq::PartMulti::pack(BROKER_SYNC_COMPL, seqn, ""sv)
                                    .withRoutingID(rouID)) == -1) {
            throw errWouldBlock;
        }
    }
    // check whether the peer has disappeared.
    catch (const err::ZMQSocketSendFailed& e) {
        switch (e.arg().toInt()) {
        case EHOSTUNREACH:
            LOG_WARN(log::Arg{"broker"sv}, log::Arg{"sync"sv, "abort"sv},
                log::Arg{"reason"sv, "host unreachable"sv});
            break;

        case EAGAIN:
            LOG_WARN(log::Arg{"broker"sv}, log::Arg{"sync"sv, "abort"sv},
                log::Arg{"reason"sv, "send would block"sv});
            break;

        default:
            throw;
        };
    }
}

} // namespace fuurin
