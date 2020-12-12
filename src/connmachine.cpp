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

ConnMachine::ConnMachine(std::string_view name, Uuid uuid, zmq::Context* zctx,
    std::chrono::milliseconds retry, std::chrono::milliseconds timeout,
    CloseFunc doClose, OpenFunc doOpen, PongFunc doPong, ChangeFunc onChange)
    : name_{name}
    , uuid_{uuid}
    , doClose_{doClose}
    , doOpen_{doOpen}
    , doPong_{doPong}
    , onChange_{onChange}
    , timerTry_{std::make_unique<zmq::Timer>(zctx, log::format("%s_conn_tmr_retry", name.data()))}
    , timerTmo_{std::make_unique<zmq::Timer>(zctx, log::format("%s_conn_tmr_timeout", name.data()))}
    , state_{State::Halted}
{
    timerTry_->setSingleShot(false);
    timerTmo_->setSingleShot(true);
    timerTry_->setInterval(retry);
    timerTmo_->setInterval(timeout);

    halt();
}


ConnMachine::~ConnMachine() noexcept = default;


std::string_view ConnMachine::name() const noexcept
{
    return name_;
}


Uuid ConnMachine::uuid() const noexcept
{
    return uuid_;
}


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


void ConnMachine::onStart()
{
    if (state_ != State::Halted)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"event"sv, "start"sv});

    trigger();
}


void ConnMachine::onStop()
{
    if (state_ == State::Halted)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"event"sv, "stop"sv});

    halt();
}


void ConnMachine::onPing()
{
    if (state_ == State::Halted)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"event"sv, "ping"sv});

    timerTry_->stop();
    timerTmo_->start();

    change(State::Stable);

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"action"sv, "pong"sv});

    doPong_();
}


void ConnMachine::onTimerRetryFired()
{
    if (timerTry_->isExpired())
        timerTry_->consume();

    if (state_ == State::Halted)
        return;

    if (state_ == State::Stable)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"event"sv, "announce"sv});

    doPong_();
}


void ConnMachine::onTimerTimeoutFired()
{
    if (timerTmo_->isExpired())
        timerTmo_->consume();

    if (state_ == State::Halted)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"event"sv, "timeout"sv});

    trigger();
}


void ConnMachine::trigger()
{
    timerTry_->start();
    timerTmo_->start();

    change(State::Trying);
    doClose_();
    doOpen_();
    doPong_();
}


void ConnMachine::halt()
{
    timerTry_->stop();
    timerTmo_->stop();

    doClose_();
    change(State::Halted);
}


void ConnMachine::change(State state)
{
    if (state_ == state)
        return;

    switch (state) {
    case State::Halted:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"trans"sv, "halted"sv});
        break;

    case State::Trying:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"trans"sv, "trying"sv});
        break;

    case State::Stable:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"trans"sv, "stable"sv});
        break;
    }

    state_ = state;
    onChange_(state);
}


std::ostream& operator<<(std::ostream& os, const ConnMachine::State& st)
{
    switch (st) {
    case ConnMachine::State::Halted:
        os << "halted";
        break;
    case ConnMachine::State::Trying:
        os << "trying";
        break;
    case ConnMachine::State::Stable:
        os << "stable";
        break;
    }
    return os;
}

} // namespace fuurin
