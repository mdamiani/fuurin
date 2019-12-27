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
#include "log.h"

#include <boost/scope_exit.hpp>


#define GROUP_EVENTS "EVN"


namespace fuurin {

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

    auto ret = std::async(std::launch::async, &Runner::run, this);

    commit = true;
    return ret;
}


void Runner::run()
{
    BOOST_SCOPE_EXIT(this)
    {
        running_ = false;
    };

    auto poll = createPoller(zopr_.get());

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

            operationReady(oper, payload);

            if (oper == Operation::Stop)
                return;
        }
    }
}


bool Runner::stop() noexcept
{
    if (!isRunning())
        return false;

    sendOperation(Operation::Stop);
    return true;
}


std::optional<zmq::Part> Runner::waitForEvent(std::chrono::milliseconds timeout)
{
    zmq::Poller poll{zmq::PollerEvents::Read, zevr_.get()};
    poll.setTimeout(timeout);
    if (poll.wait().empty())
        return {};

    auto [ev, valid] = recvEvent();
    if (!valid)
        return {};

    return std::move(ev);
}


std::unique_ptr<zmq::PollerWaiter> Runner::createPoller(zmq::Socket* sock)
{
    return std::unique_ptr<zmq::PollerWaiter>{new zmq::PollerAuto{zmq::PollerEvents::Type::Read, sock}};
}


void Runner::operationReady(oper_type_t, zmq::Part&)
{
}


void Runner::socketReady(zmq::Socket*)
{
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


std::tuple<Runner::oper_type_t, zmq::Part, bool> Runner::recvOperation() noexcept
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


void Runner::sendEvent(zmq::Part&& ev)
{
    zevs_->send(zmq::PartMulti::pack(token_.load(), ev).withGroup(GROUP_EVENTS));
}


std::tuple<zmq::Part, bool> Runner::recvEvent()
{
    zmq::Part r;
    zevr_->recv(&r);
    auto [tok, ev] = zmq::PartMulti::unpack<token_type_t, zmq::Part>(r);

    return std::make_tuple(std::move(ev), tok == token_);
}


zmq::Context* Runner::zmqContext() const noexcept
{
    return zctx_.get();
}


} // namespace fuurin
