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
#include "fuurin/workerconfig.h"
#include "syncmachine.h"
#include "failure.h"
#include "types.h"
#include "log.h"

#include <memory>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <string>
#include <algorithm>


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

Broker::Broker(Uuid id)
    : Runner(id)
{
}


Broker::~Broker() noexcept
{
}


zmq::Part Broker::prepareConfiguration() const
{
    return BrokerConfig{
        uuid(),
        endpointDelivery(),
        endpointDispatch(),
        endpointSnapshot(),
    }
        .toPart();
}


std::unique_ptr<Runner::Session> Broker::createSession(CompletionFunc onComplete) const
{
    return makeSession<BrokerSession>(onComplete);
}


/**
 * ASYNC TASK
 */

Broker::BrokerSession::BrokerSession(Uuid id, token_type_t token, CompletionFunc onComplete,
    zmq::Context* zctx, zmq::Socket* zoper, zmq::Socket* zevent)
    : Session(id, token, onComplete, zctx, zoper, zevent)
    , zsnapshot_{std::make_unique<zmq::Socket>(zctx, zmq::Socket::SERVER)}
    , zdelivery_{std::make_unique<zmq::Socket>(zctx, zmq::Socket::DISH)}
    , zdispatch_{std::make_unique<zmq::Socket>(zctx, zmq::Socket::RADIO)}
    , zhugz_{std::make_unique<zmq::Timer>(zctx, "hugz")}
    , storTopic_{1024} // TODO: configure capacity.
    , storWorker_{64}  // TODO: configure capacity.
{
    zhugz_->setInterval(1s);
    zhugz_->setSingleShot(false);
}


Broker::BrokerSession::~BrokerSession() noexcept = default;


std::unique_ptr<zmq::PollerWaiter> Broker::BrokerSession::createPoller()
{
    return std::unique_ptr<zmq::PollerWaiter>{new zmq::PollerAuto{zmq::PollerEvents::Type::Read,
        zopr_, zsnapshot_.get(), zdelivery_.get(), zhugz_.get()}};
}


void Broker::BrokerSession::saveConfiguration(const zmq::Part& part)
{
    conf_ = BrokerConfig::fromPart(part);
}


void Broker::BrokerSession::openSockets()
{
    zdelivery_->setEndpoints({conf_.endpDispatch.begin(), conf_.endpDispatch.end()});
    zdispatch_->setEndpoints({conf_.endpDelivery.begin(), conf_.endpDelivery.end()});
    zsnapshot_->setEndpoints({conf_.endpSnapshot.begin(), conf_.endpSnapshot.end()});

    zdelivery_->setGroups({WORKER_HUGZ, WORKER_UPDT});

    zdelivery_->bind();
    zdispatch_->bind();
    zsnapshot_->bind();
}


void Broker::BrokerSession::closeSockets()
{
    zdelivery_->close();
    zdispatch_->close();
    zsnapshot_->close();
}


void Broker::BrokerSession::operationReady(Operation* oper)
{
    switch (oper->type()) {
    case Operation::Type::Start:
        LOG_DEBUG(log::Arg{"broker"sv, uuid_.toShortString()}, log::Arg{"started"sv});
        saveConfiguration(oper->payload());
        openSockets();
        break;

    case Operation::Type::Stop:
        LOG_DEBUG(log::Arg{"broker"sv, uuid_.toShortString()}, log::Arg{"stopped"sv});
        closeSockets();
        break;

    default:
        LOG_ERROR(log::Arg{"broker"sv, uuid_.toShortString()},
            log::Arg{"operation"sv, Operation::toString(oper->type())},
            log::Arg{"unknown"sv});
        break;
    }
}


void Broker::BrokerSession::socketReady(zmq::Pollable* pble)
{
    if (pble == zsnapshot_.get()) {
        zmq::Part payload;
        zsnapshot_->recv(&payload);
        LOG_DEBUG(log::Arg{"broker"sv, uuid_.toShortString()}, log::Arg{"snapshot"sv},
            log::Arg{"size"sv, int(payload.size())});

        receiveWorkerCommand(std::move(payload));

    } else if (pble == zdelivery_.get()) {
        zmq::Part payload;
        zdelivery_->recv(&payload);
        LOG_DEBUG(log::Arg{"broker"sv, uuid_.toShortString()}, log::Arg{"delivery"sv},
            log::Arg{"size"sv, int(payload.size())});

        collectWorkerMessage(std::move(payload));

    } else if (pble == zhugz_.get()) {
        zhugz_->consume();
        sendHugz();

    } else {
        LOG_FATAL(log::Arg{"broker"sv, uuid_.toShortString()},
            log::Arg{"could not read ready socket"sv},
            log::Arg{"unknown socket"sv});
    }
}


void Broker::BrokerSession::collectWorkerMessage(zmq::Part&& payload)
{
    if (std::strncmp(payload.group(), WORKER_HUGZ, sizeof(WORKER_HUGZ)) == 0) {
        // TODO: extract the message
        if (!zhugz_->isActive())
            zhugz_->start();

    } else if (std::strncmp(payload.group(), WORKER_UPDT, sizeof(WORKER_UPDT)) == 0) {
        const auto t = Topic::fromPart(payload).withBroker(uuid_);

        if (!storeTopic(t)) {
            LOG_DEBUG(log::Arg{"broker"sv, uuid_.toShortString()}, log::Arg{"discard"sv},
                log::Arg{"from"sv, t.worker().toShortString()},
                log::Arg{"name"sv, std::string_view(t.name())},
                log::Arg{"seqn"sv, int(t.seqNum())}, // FIXME: fix cast to int.
                log::Arg{"size"sv, int(t.data().size())});
            return;
        }

        LOG_DEBUG(log::Arg{"broker"sv, uuid_.toShortString()}, log::Arg{"dispatch"sv},
            log::Arg{"from"sv, t.worker().toShortString()},
            log::Arg{"name"sv, std::string_view(t.name())},
            log::Arg{"seqn"sv, int(t.seqNum())}, // FIXME: fix cast to int.
            log::Arg{"size"sv, int(t.data().size())});

        /**
         * Topic is sent twice, both to the global group
         * and to the topic name group (topic name is
         * a null terminated string).
         */
        // TODO: Is it possible to avoid sending topic twice?
        zdispatch_->send(t.toPart().withGroup(std::string_view(t.name()).data()));
        zdispatch_->send(t.toPart().withGroup(BROKER_UPDT));

    } else {
        LOG_WARN(log::Arg{"broker"sv, uuid_.toShortString()},
            log::Arg{"collect"sv, "recv"sv},
            log::Arg{"group"sv, std::string(payload.group())},
            log::Arg{"unknown message"sv});
    }
}


bool Broker::BrokerSession::storeTopic(const Topic& t)
{
    auto it = storTopic_.find(t.name());

    if (it == storTopic_.list().end()) {
        // TODO: configure capacity.
        it = storTopic_.put(t.name(), LRUCache<WorkerUuid, Topic>{8});
    }

    ASSERT(it != storTopic_.list().end(), "broker storage topic cache is null");

    Topic::SeqN lastSeqNum{0};
    if (auto el = storWorker_.find(t.worker()); el != storWorker_.list().end())
        lastSeqNum = el->second;

    // filter out old topics.
    if (t.seqNum() <= lastSeqNum)
        return false;

    storWorker_.put(t.worker(), t.seqNum());

    auto it2 = it->second.put(t.worker(), t);

    ASSERT(it2 != it->second.list().end(), "broker storage uuid cache is null");

    return true;
}


void Broker::BrokerSession::sendHugz()
{
    // TODO: send network status update.
    zdispatch_->send(zmq::Part{}.withGroup(BROKER_HUGZ));
}


void Broker::BrokerSession::receiveWorkerCommand(zmq::Part&& payload)
{
    // TODO: use snapshot payload
    auto [req, syncseq, params] = zmq::PartMulti::unpack<std::string_view, SyncMachine::seqn_t, zmq::Part>(payload);

    if (req != BROKER_SYNC_REQST) {
        LOG_WARN(log::Arg{"broker"sv, uuid_.toShortString()},
            log::Arg{"snapshot"sv, "recv"sv},
            log::Arg{"request"sv, req},
            log::Arg{"syncseq"sv, syncseq},
            log::Arg{"unknown request"sv});

        return;
    }

    replySnapshot(payload.routingID(), syncseq, std::move(params));
}


void Broker::BrokerSession::replySnapshot(uint32_t rouID, uint8_t syncseq, zmq::Part&& params)
{
    static_assert(std::is_same_v<SyncMachine::seqn_t, decltype(syncseq)>);

    LOG_DEBUG(log::Arg{"broker"sv, uuid_.toShortString()}, log::Arg{"sync"sv, "reply"sv},
        log::Arg{"elements"sv, int(storTopic_.size())});

    const WorkerConfig conf = WorkerConfig::fromPart(params);

    try {
        const auto& errWouldBlock = ERROR(ZMQSocketSendFailed, "",
            log::Arg{log::ec_t{EAGAIN}});

        if (zsnapshot_->trySend(zmq::PartMulti::pack(BROKER_SYNC_BEGIN, syncseq, uuid_.toPart())
                                    .withRoutingID(rouID)) == -1) {
            throw errWouldBlock;
        }
        for (const auto& el : storTopic_.list()) {
            ASSERT(!el.second.list().empty(), "topic entry has empty cache");
            const auto& t = el.second.list().back().second;

            if (!conf.topicsAll && std::find(conf.topicsNames.begin(), conf.topicsNames.end(), t.name()) == conf.topicsNames.end())
                continue;

            if (zsnapshot_->trySend(zmq::PartMulti::pack(BROKER_SYNC_ELEMN, syncseq, t.toPart())
                                        .withRoutingID(rouID)) == -1) {
                throw errWouldBlock;
            }
        }
        if (zsnapshot_->trySend(zmq::PartMulti::pack(BROKER_SYNC_COMPL, syncseq, uuid_.toPart())
                                    .withRoutingID(rouID)) == -1) {
            throw errWouldBlock;
        }
    }
    // check whether the peer has disappeared.
    catch (const err::ZMQSocketSendFailed& e) {
        switch (e.arg().toInt()) {
        case EHOSTUNREACH:
            LOG_WARN(log::Arg{"broker"sv, uuid_.toShortString()},
                log::Arg{"sync"sv, "abort"sv},
                log::Arg{"reason"sv, "host unreachable"sv});
            break;

        case EAGAIN:
            LOG_WARN(log::Arg{"broker"sv, uuid_.toShortString()},
                log::Arg{"sync"sv, "abort"sv},
                log::Arg{"reason"sv, "send would block"sv});
            break;

        default:
            throw;
        };
    }
}

} // namespace fuurin
