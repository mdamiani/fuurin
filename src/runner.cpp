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
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"
#include "failure.h"
#include "types.h"
#include "log.h"

#include <boost/scope_exit.hpp>

#include <cstring>


#define GROUP_EVENTS "EVN"


namespace fuurin {

/**
 * MAIN TASK
 */

Runner::Runner()
    : zctx_(std::make_unique<zmq::Context>())
    , zops_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR))
    , zopr_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR))
    , zevs_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::RADIO))
    , zevr_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::DISH))
    , running_(false)
    , token_(0)
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

    // TODO: check whether RADIO/DISH over inproc has the slow join behavior,
    //       in order to avoid missing notifications.
}


Runner::~Runner() noexcept
{
    stop();
}


bool Runner::isRunning() const noexcept
{
    return running_;
}


std::unique_ptr<Runner::Session> Runner::makeSession(std::function<void()> onComplete) const
{
    return std::make_unique<Session>(token_, onComplete, zctx_, zopr_, zevs_);
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
        [s = makeSession([this]() {
            running_ = false;
        })]() {
            s->run();
        });

    commit = true;
    return ret;
}


bool Runner::stop() noexcept
{
    if (!isRunning())
        return false;

    sendOperation(toIntegral(Operation::Stop));
    return true;
}


std::tuple<Runner::event_type_t, zmq::Part, Runner::EventRead> Runner::waitForEvent(std::chrono::milliseconds timeout)
{
    zmq::Poller poll{zmq::PollerEvents::Read, timeout, zevr_.get()};

    if (poll.wait().empty())
        return std::make_tuple(toIntegral(EventType::Invalid), zmq::Part{}, EventRead::Timeout);

    auto [ev, pay, valid] = recvEvent();
    if (!valid)
        return std::make_tuple(ev, std::move(pay), EventRead::Discard);

    return std::make_tuple(ev, std::move(pay), EventRead::Success);
}


std::tuple<Runner::event_type_t, zmq::Part, bool> Runner::recvEvent()
{
    zmq::Part r;
    zevr_->recv(&r);
    ASSERT(std::strncmp(r.group(), GROUP_EVENTS, sizeof(GROUP_EVENTS)) == 0, "bad event group");

    auto [tok, ev, pay] = zmq::PartMulti::unpack<token_type_t, event_type_t, zmq::Part>(r);

    return std::make_tuple(ev, std::move(pay), tok == token_);
}


void Runner::sendOperation(oper_type_t oper) noexcept
{
    sendOperation(oper, zmq::Part());
}


void Runner::sendOperation(oper_type_t oper, zmq::Part&& payload) noexcept
{
    try {
        zops_->send(zmq::Part{token_}, zmq::Part{oper}, payload);
    } catch (const std::exception& e) {
        LOG_FATAL(log::Arg{"runner"sv}, log::Arg{"operation send threw exception"sv},
            log::Arg{std::string_view(e.what())});
    }
}


/**
 * ASYNC TASK
 */

Runner::Session::Session(token_type_t token, std::function<void()> oncompl,
    const std::unique_ptr<zmq::Context>& zctx,
    const std::unique_ptr<zmq::Socket>& zoper,
    const std::unique_ptr<zmq::Socket>& zevent)
    : token_(token)
    , docompl_(oncompl)
    , zctx_(zctx)
    , zopr_(zoper)
    , zevs_(zevent)
{
}


Runner::Session::~Session() noexcept = default;


void Runner::Session::run()
{
    BOOST_SCOPE_EXIT(this)
    {
        try {
            if (docompl_)
                docompl_();
        } catch (...) {
            LOG_FATAL(log::Arg{"runner"sv},
                log::Arg{"session on complete action threw exception"sv});
        }
    };

    auto poll = createPoller();

    // generate a start operation
    operationReady(toIntegral(Operation::Start), zmq::Part{});

    for (;;) {
        for (auto s : poll->wait()) {
            if (s != zopr_.get()) {
                socketReady(s);
                continue;
            }

            auto [oper, payload, valid] = recvOperation();

            // filter out old operations
            if (!valid)
                continue;

            operationReady(oper, std::move(payload));

            if (oper == toIntegral(Operation::Stop))
                return;
        }
    }
}


std::unique_ptr<zmq::PollerWaiter> Runner::Session::createPoller()
{
    auto poll = new zmq::Poller{zmq::PollerEvents::Type::Read, zopr_.get()};
    return std::unique_ptr<zmq::PollerWaiter>{poll};
}


void Runner::Session::operationReady(oper_type_t, zmq::Part&&)
{
}


void Runner::Session::socketReady(zmq::Pollable*)
{
}


std::tuple<Runner::oper_type_t, zmq::Part, bool> Runner::Session::recvOperation() noexcept
{
    zmq::Part tok, oper, payload;

    try {
        zopr_->recv(&tok, &oper, &payload);
    } catch (const std::exception& e) {
        LOG_FATAL(log::Arg{"runner"sv}, log::Arg{"operation recv threw exception"sv},
            log::Arg{std::string_view(e.what())});
    }

    return std::make_tuple(oper.toUint8(), std::move(payload), tok.toUint8() == token_);
}


void Runner::Session::sendEvent(event_type_t ev, zmq::Part&& pay)
{
    zevs_->send(zmq::PartMulti::pack(token_, ev, pay).withGroup(GROUP_EVENTS));
}

} // namespace fuurin
