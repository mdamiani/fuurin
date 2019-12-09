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
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "log.h"

#include <boost/scope_exit.hpp>


namespace fuurin {

Worker::Worker()
    : zctx_(std::make_unique<zmq::Context>())
    , zcmds_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR))
    , zcmdr_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR))
    , running_(false)
    , token_(0)
{
    zcmds_->setEndpoints({"inproc://worker-loop"});
    zcmdr_->setEndpoints({"inproc://worker-loop"});

    zcmdr_->bind();
    zcmds_->connect();
}


Worker::~Worker() noexcept
{
    stop();
}


std::future<void> Worker::start()
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

    auto ret = std::async(std::launch::async, &Worker::run, this, token_);

    commit = true;
    return ret;
}


void Worker::run(uint8_t token)
{
    BOOST_SCOPE_EXIT(this)
    {
        running_ = false;
    };

    zmq::Poller poll(zmq::PollerEvents::Type::Read, zcmdr_.get());

    for (;;) {
        for (auto s : poll.wait()) {
            zmq::Part tok, cmd;
            s->recv(&tok, &cmd);
            if (tok.toUint8() != token)
                // filter out old commands
                continue;

            switch (cmd.toUint8()) {
            case Command::Stop:
                return;
                break;
            }
        }
    }
}


bool Worker::stop() noexcept
{
    if (!isRunning())
        return false;

    try {
        zcmds_->send(zmq::Part(token_), zmq::Part(Command::Stop));
    } catch (const std::exception& e) {
        LOG_FATAL(log::Arg{"Worker stop failure"sv},
            log::Arg{std::string_view(e.what())});
    }

    return true;
}


bool Worker::isRunning() const noexcept
{
    return running_;
}

} // namespace fuurin
