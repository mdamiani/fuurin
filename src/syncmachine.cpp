/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "syncmachine.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqtimer.h"
#include "failure.h"
#include "log.h"

#include <limits>


using namespace std::literals::string_view_literals;


namespace fuurin {

SyncMachine::SyncMachine(std::string_view name, Uuid uuid, zmq::Context* zctx,
    int maxIndex, int maxRetry, std::chrono::milliseconds timeout,
    CloseFunc close, OpenFunc open, SyncFunc sync, ChangeFunc change)
    : name_{name}
    , uuid_{uuid}
    , indexMax_{maxIndex}
    , retryMax_{maxRetry}
    , doClose_{close}
    , doOpen_{open}
    , doSync_{sync}
    , onChange_{change}
    , timerTmo_{std::make_unique<zmq::Timer>(zctx, log::format("%s_sync_tmr_timeout", name.data()))}
    , state_{State::Halted}
    , indexCurr_{0}
    , indexNext_{0}
    , retryCurr_{0}
    , seqNum_{0}
{
    ASSERT(indexMax_ >= 0, "SyncMachine max index cannot be negative");
    ASSERT(retryMax_ >= 0, "SyncMachine max retry cannot be negative");
    ASSERT(indexMax_ < std::numeric_limits<int>::max(), "SyncMachine max index too big");
    ASSERT(retryMax_ < std::numeric_limits<int>::max(), "SyncMachine max retry too big");

    timerTmo_->setSingleShot(true);
    timerTmo_->setInterval(timeout);

    for (int idx = 0; idx <= indexMax_; ++idx)
        doClose_(idx);

    halt(-1);
}


SyncMachine::~SyncMachine() noexcept = default;


std::string_view SyncMachine::name() const noexcept
{
    return name_;
}


Uuid SyncMachine::uuid() const noexcept
{
    return uuid_;
}


SyncMachine::State SyncMachine::state() const noexcept
{
    return state_;
}


zmq::Timer* SyncMachine::timerTimeout() const noexcept
{
    return timerTmo_.get();
}


int SyncMachine::maxRetry() const noexcept
{
    return retryMax_;
}


int SyncMachine::maxIndex() const noexcept
{
    return indexMax_;
}


void SyncMachine::setNextIndex(int index) noexcept
{
    if (index < 0)
        index = 0;

    indexNext_ = index % (indexMax_ + 1);
}


int SyncMachine::nextIndex() const noexcept
{
    return indexNext_;
}


int SyncMachine::currentIndex() const noexcept
{
    return indexCurr_;
}


int SyncMachine::retryCount() const noexcept
{
    return retryCurr_;
}


SyncMachine::seqn_t SyncMachine::sequenceNumber() const noexcept
{
    return seqNum_;
}


void SyncMachine::onHalt()
{
    if (state_ == State::Halted)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"event"sv, "halt"sv});

    switch (state_) {
    case State::Failed:
        halt(-1);
        break;

    case State::Download:
    case State::Synced:
        halt(indexCurr_);
        break;

    default:
        break;
    };
}


void SyncMachine::onSync()
{
    if (state_ == State::Download)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"event"sv, "sync"sv});

    retryCurr_ = 0;

    switch (state_) {
    case State::Failed:
        indexCurr_ = indexNext_;
        setNextIndex(indexCurr_ + 1);
        // fallthrough

    case State::Halted:
        sync(-1, indexCurr_);
        break;

    case State::Synced:
        sync(-1, -1);
        break;

    default:
        break;
    };
}


SyncMachine::ReplyResult SyncMachine::onReply(int index, seqn_t seqn, ReplyType reply)
{
    if (state_ != State::Download)
        return ReplyResult::Unexpected;

    if (index != indexCurr_ || seqn != seqNum_)
        return ReplyResult::Discarded;

    switch (reply) {
    case ReplyType::Snapshot:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"event"sv, "reply"sv},
            log::Arg{"index"sv, index}, log::Arg{"seqn"sv, seqn},
            log::Arg{"type"sv, "snapshot"sv});

        timerTmo_->start();
        break;

    case ReplyType::Complete:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"event"sv, "reply"sv},
            log::Arg{"index"sv, index}, log::Arg{"seqn"sv, seqn},
            log::Arg{"type"sv, "complete"sv});

        timerTmo_->stop();
        change(State::Synced);
        break;
    }

    return ReplyResult::Accepted;
}


void SyncMachine::onTimerTimeoutFired()
{
    if (timerTmo_->isExpired())
        timerTmo_->consume();

    if (state_ != State::Download)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"event"sv, "timeout"sv});

    if (retryCurr_ + 1 > retryMax_) {
        fail(indexCurr_);
        return;
    }

    ++retryCurr_;
    const auto indexPrev = indexCurr_;
    indexCurr_ = indexNext_;
    setNextIndex(indexCurr_ + 1);

    sync(indexPrev, indexCurr_);
}


void SyncMachine::halt(int indexClose)
{
    timerTmo_->stop();

    seqNum_ = 0;
    retryCurr_ = 0;
    indexCurr_ = 0;
    setNextIndex(1);

    close(indexClose);
    change(State::Halted);
}


void SyncMachine::fail(int indexClose)
{
    timerTmo_->stop();

    close(indexClose);
    change(State::Failed);
}


void SyncMachine::sync(int indexClose, int indexOpen)
{
    timerTmo_->start();

    ++seqNum_;

    change(State::Download);
    close(indexClose);
    open(indexOpen);

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"action"sv, "sync"sv},
        log::Arg{"index"sv, indexCurr_}, log::Arg{"seqn"sv, seqNum_});

    doSync_(indexCurr_, seqNum_);
}


void SyncMachine::close(int index)
{
    if (index < 0)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"action"sv, "close"sv},
        log::Arg{"index"sv, index});

    doClose_(index);
}


void SyncMachine::open(int index)
{
    if (index < 0)
        return;

    LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"action"sv, "open"sv},
        log::Arg{"index"sv, index});

    doOpen_(index);
}


void SyncMachine::change(State state)
{
    if (state_ == state)
        return;

    switch (state) {
    case State::Halted:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"trans"sv, "halted"sv});
        break;

    case State::Download:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"trans"sv, "download"sv});
        break;

    case State::Failed:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"trans"sv, "failed"sv});
        break;

    case State::Synced:
        LOG_DEBUG(log::Arg{name_, uuid_.toShortString()}, log::Arg{"trans"sv, "synced"sv});
        break;
    }

    state_ = state;
    onChange_(state);
}


std::ostream& operator<<(std::ostream& os, const SyncMachine::State& en)
{
    switch (en) {
    case SyncMachine::State::Halted:
        os << "halted"sv;
        break;
    case SyncMachine::State::Download:
        os << "download"sv;
        break;
    case SyncMachine::State::Failed:
        os << "failed"sv;
        break;
    case SyncMachine::State::Synced:
        os << "synced"sv;
        break;
    }
    return os;
}


std::ostream& operator<<(std::ostream& os, const SyncMachine::ReplyType& en)
{
    switch (en) {
    case SyncMachine::ReplyType::Snapshot:
        os << "snapshot"sv;
        break;
    case SyncMachine::ReplyType::Complete:
        os << "complete"sv;
        break;
    }
    return os;
}


std::ostream& operator<<(std::ostream& os, const SyncMachine::ReplyResult& en)
{
    switch (en) {
    case SyncMachine::ReplyResult::Unexpected:
        os << "unexpected"sv;
        break;
    case SyncMachine::ReplyResult::Discarded:
        os << "discarded"sv;
        break;
    case SyncMachine::ReplyResult::Accepted:
        os << "accepted"sv;
        break;
    }
    return os;
}

} // namespace fuurin
