/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/runner.h"
#include "fuurin/session.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"
#include "fuurin/zmqcancel.h"
#include "failure.h"
#include "log.h"

#include <zmq.h>

#include <boost/scope_exit.hpp>

#include <cstring>


#define GROUP_EVENTS "EVN"


namespace fuurin {


Runner::Runner(Uuid id, const std::string& name)
    : name_{name}
    , uuid_(id)
    , zctx_(std::make_unique<zmq::Context>())
    , zops_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR))
    , zopr_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR))
    , zevs_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::RADIO))
    , zevr_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::DISH))
    , zfins_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PUSH)}
    , zfinr_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PULL)}
    , zevpoll_{std::make_unique<zmq::PollerAuto<zmq::Socket>>(zmq::PollerEvents::Read, 0ms, zevr_.get())}
    , running_(false)
    , token_(0)
    , endpDelivery_{{"ipc:///tmp/worker_delivery"}}
    , endpDispatch_{{"ipc:///tmp/worker_dispatch"}}
    , endpSnapshot_{{"ipc:///tmp/broker_snapshot"}}
{
    zops_->setEndpoints({"inproc://runner-loop"});
    zopr_->setEndpoints({"inproc://runner-loop"});

    zevs_->setEndpoints({"inproc://runner-events"});
    zevr_->setEndpoints({"inproc://runner-events"});

    zevr_->setGroups({GROUP_EVENTS});

    zopr_->bind();
    zops_->connect();

    zevs_->bind();
    zevr_->connect();

    // MUST be inproc in order to get instant delivery of messages.
    zfins_->setEndpoints({"inproc://runner-terminate"});
    zfinr_->setEndpoints({"inproc://runner-terminate"});

    zfins_->setHighWaterMark(1, 1);
    zfinr_->setHighWaterMark(1, 1);

    zfins_->setConflate(true);
    zfinr_->setConflate(true);

    zfinr_->bind();
    zfins_->connect();

    // FIXME: to be removed.
    zevpoll_->wait();
}


Runner::~Runner() noexcept
{
    stop();
}


std::string_view Runner::name() const
{
    return std::string_view(name_.data(), name_.size());
}


Uuid Runner::uuid() const
{
    return uuid_;
}


void Runner::setEndpoints(const std::vector<std::string>& delivery,
    const std::vector<std::string>& dispatch,
    const std::vector<std::string>& snapshot)
{
    endpDelivery_ = delivery;
    endpDispatch_ = dispatch;
    endpSnapshot_ = snapshot;
}


const std::vector<std::string>& Runner::endpointDelivery() const
{
    return endpDelivery_;
}


const std::vector<std::string>& Runner::endpointDispatch() const
{
    return endpDispatch_;
}


const std::vector<std::string>& Runner::endpointSnapshot() const
{
    return endpSnapshot_;
}


bool Runner::isRunning() const noexcept
{
    // latest message is always available
    // over the socket, because we are using
    // inproc transport.
    zmq::Part r;
    if (zfinr_->tryRecv(&r) != -1) {
        if (r.toUint8() == token_)
            running_ = false;
    }

    return running_;
}


zmq::Context* Runner::context() const noexcept
{
    return zctx_.get();
}


zmq::Part Runner::prepareConfiguration() const
{
    return zmq::Part{};
}


std::unique_ptr<Session> Runner::createSession() const
{
    return makeSession<Session>();
}


std::future<void> Runner::start()
{
    if (isRunning())
        return std::future<void>();

    ++token_;
    running_ = true;

    bool commit = false;
    BOOST_SCOPE_EXIT(this, &commit)
    {
        if (!commit)
            running_ = false;
    };

    auto ret = std::async(std::launch::async,
        [s = createSession()]() {
            s->run();
        });

    sendOperation(Operation::Type::Start, prepareConfiguration());

    commit = true;
    return ret;
}


bool Runner::stop() noexcept
{
    if (!isRunning())
        return false;

    sendOperation(Operation::Type::Stop);
    return true;
}


Event Runner::waitForEvent(std::chrono::milliseconds timeout, EventMatchFunc match) const
{
    return waitForEvent(
        zmq::Cancellation{zctx_.get(), "waitForEvent_canc_deadline"}
            .withDeadline(timeout),
        match);
}


Event Runner::waitForEvent(zmq::Pollable& canc, EventMatchFunc match) const
{
    return waitForEvent(&canc, match);
}


Event Runner::waitForEvent(zmq::Pollable* canc, EventMatchFunc match) const
{
    // FIXME: to be removed.
    BOOST_SCOPE_EXIT(this)
    {
        zevpoll_->wait();
    };

    zmq::Poller pw{zmq::PollerEvents::Read, zevr_.get(), canc};

    for (;;) {
        for (auto s : pw.wait()) {
            if (s == canc)
                return {Event::Type::Invalid, Event::Notification::Timeout};

            const auto& ev = recvEvent();

            if (ev.notification() == Event::Notification::Timeout ||
                (match && !match(ev.type()))) //
            {
                continue;
            }

            return ev;
        }
    }
}


int Runner::eventFD() const
{
    zmq_fd_t fd;
    const int rc = zmq_poller_fd(zevpoll_->zmqPointer(), &fd);

    // if it fails then it's an internal error.
    ASSERT(rc == 0, "failed to get events socket file descriptor");

    return fd;
}


Event Runner::recvEvent() const
{
    zmq::Part r;
    const auto n = zevr_->tryRecv(&r);
    if (n == -1) {
        return {Event::Type::Invalid, Event::Notification::Timeout};
    }

    ASSERT(std::strncmp(r.group(), GROUP_EVENTS, sizeof(GROUP_EVENTS)) == 0, "bad event group");

    auto [tok, ev] = zmq::PartMulti::unpack<SessionEnv::token_t, std::string_view>(r);

    return Event::fromPart(ev)
        .withNotification(tok == token_
                ? Event::Notification::Success
                : Event::Notification::Discard);
}


void Runner::sendOperation(Operation::Type oper) noexcept
{
    sendOperation(oper, zmq::Part());
}


void Runner::sendOperation(Operation::Type oper, zmq::Part&& payload) noexcept
{
    sendOperation(zops_.get(), token_, oper, std::move(payload));
}


void Runner::sendOperation(zmq::Socket* sock, SessionEnv::token_t token, Operation::Type oper, zmq::Part&& payload) noexcept
{
    try {
        sock->send(zmq::Part{token},
            Operation{oper, Operation::Notification::Success, payload}
                .toPart());
    } catch (const std::exception& e) {
        LOG_FATAL(log::Arg{"runner"sv}, log::Arg{"operation send threw exception"sv},
            log::Arg{std::string_view(e.what())});
    }
}

} // namespace fuurin
