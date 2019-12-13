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
#include "log.h"

#include <boost/scope_exit.hpp>


namespace fuurin {

Runner::Runner()
    : zctx_(std::make_unique<zmq::Context>())
    , zops_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR))
    , zopr_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR))
    , running_(false)
    , token_(0)
{
    zops_->setEndpoints({"inproc://runner-loop"});
    zopr_->setEndpoints({"inproc://runner-loop"});

    zopr_->bind();
    zops_->connect();
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

    auto ret = std::async(std::launch::async, &Runner::run, this, token_);

    commit = true;
    return ret;
}


void Runner::run(token_type_t token)
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

            auto [tok, oper, payload] = recvOperation();

            // filter out old operations
            if (tok != token)
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


std::unique_ptr<zmq::PollerWaiter> Runner::createPoller(zmq::Socket* sock)
{
    return std::unique_ptr<zmq::PollerWaiter>(new zmq::Poller(zmq::PollerEvents::Type::Read, sock));
}


void Runner::operationReady(oper_type_t, const zmq::Part&)
{
}


void Runner::socketReady(zmq::Socket*)
{
}


void Runner::sendOperation(oper_type_t oper) noexcept
{
    zmq::Part empty;
    sendOperation(oper, empty);
}


void Runner::sendOperation(oper_type_t oper, zmq::Part& payload) noexcept
{
    try {
        zops_->send(zmq::Part(token_), zmq::Part(oper), payload);
    } catch (const std::exception& e) {
        LOG_FATAL(log::Arg{"Runner operation send failure"sv},
            log::Arg{std::string_view(e.what())});
    }
}


std::tuple<Runner::token_type_t, Runner::oper_type_t, zmq::Part> Runner::recvOperation() noexcept
{
    zmq::Part tok, oper, payload;

    try {
        zopr_->recv(&tok, &oper, &payload);
    } catch (const std::exception& e) {
        LOG_FATAL(log::Arg{"Runner operation recv failure"sv},
            log::Arg{std::string_view(e.what())});
    }

    return std::make_tuple(tok.toUint8(), oper.toUint8(), std::move(payload));
}


} // namespace fuurin
