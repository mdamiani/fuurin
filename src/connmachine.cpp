/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "connmachine.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqtimer.h"
#include "fuurin/errors.h"
#include "log.h"


namespace fuurin {

ConnMachine::ConnMachine(zmq::Context* zctx,
    std::chrono::milliseconds retry,
    std::chrono::milliseconds timeout,
    std::function<void()> doReset,
    std::function<void()> doPong,
    std::function<void(State)> onChange)
    : doReset_{doReset}
    , doPong_{doPong}
    , onChange_{onChange}
    , timerTry_{std::make_unique<zmq::Timer>(zctx, "conn_tmr_retry")}
    , timerTmo_{std::make_unique<zmq::Timer>(zctx, "conn_tmr_timeout")}
    , state_{State::Trying}
{
    timerTry_->setSingleShot(false);
    timerTmo_->setSingleShot(true);
    timerTry_->setInterval(retry);
    timerTmo_->setInterval(timeout);

    timerTry_->start();
    timerTmo_->start();

    change(State::Trying);
    reconnect();
    pong();
}


ConnMachine::~ConnMachine() noexcept = default;


ConnMachine::State ConnMachine::state() const noexcept
{
    return state_;
}


zmq::Timer* ConnMachine::timerRetry() const noexcept
{
    return timerTry_.get();
}


zmq::Timer* ConnMachine::timerTimeout() const noexcept
{
    return timerTmo_.get();
}


void ConnMachine::onPing()
{
    LOG_DEBUG(log::Arg{"connection"sv}, log::Arg{"event"sv, "ping"sv});

    timerTry_->stop();
    timerTmo_->start();

    change(State::Stable);
    pong();
}


void ConnMachine::onTimerRetryFired()
{
    if (state_ != State::Trying) {
        throw ERROR(Error, "connmachine could not handle onTimerRetryFired",
            log::Arg{"reason"sv, "state must be State::Trying"sv});
    }

    LOG_DEBUG(log::Arg{"connection"sv}, log::Arg{"event"sv, "retry"sv});

    if (timerTry_->isExpired())
        timerTry_->consume();

    pong();
}


void ConnMachine::onTimerTimeoutFired()
{
    LOG_DEBUG(log::Arg{"connection"sv}, log::Arg{"event"sv, "timeout"sv});

    if (timerTmo_->isExpired())
        timerTmo_->consume();

    timerTry_->start();
    timerTmo_->start();

    change(State::Trying);
    reconnect();
    pong();
}


void ConnMachine::pong()
{
    LOG_DEBUG(log::Arg{"connection"sv}, log::Arg{"event"sv, "pong"sv});

    doPong_();
}


void ConnMachine::reconnect()
{
    LOG_DEBUG(log::Arg{"connection"sv}, log::Arg{"event"sv, "reset"sv});

    doReset_();
}


void ConnMachine::change(State state)
{
    if (state_ == state)
        return;

    switch (state) {
    case State::Trying:
        LOG_DEBUG(log::Arg{"connection"sv}, log::Arg{"event"sv, "transition"sv},
            log::Arg{"state"sv, "trying"sv});
        break;

    case State::Stable:
        LOG_DEBUG(log::Arg{"connection"sv}, log::Arg{"event"sv, "transition"sv},
            log::Arg{"state"sv, "stable"sv});
        break;
    }

    state_ = state;
    onChange_(state);
}

} // namespace fuurin
