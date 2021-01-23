/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/zmqtimer.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "log.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/scope_exit.hpp>

#include <tuple>
#include <functional>


using namespace std::literals::string_literals;


namespace fuurin {
namespace zmq {


/**
 * \brief ASIO timer implementation.
 */
class Timer::IOSteadyTimer
{
public:
    /**
     * \brief Creates a new timer along with its completion future.
     */
    static std::tuple<std::future<bool>, IOSteadyTimer*> makeIOSteadyTimer(Context* const ctx,
        std::chrono::milliseconds interval, bool singleshot, Socket* trigger)
    {
        auto timer = new IOSteadyTimer(ctx, interval, singleshot, trigger);
        return std::make_tuple(timer->cancelPromise.get_future(), timer);
    }

    /**
     * \brief Timer expiration management.
     * \param[in] timer Timer to work on.
     * \param[in] ec Error code.
     */
    static void timerFired(IOSteadyTimer* timer, const boost::system::error_code& ec)
    {
        bool done = true;
        BOOST_SCOPE_EXIT(timer, &done)
        {
            if (done) {
                try {
                    timer->cancelPromise.set_value(true);
                } catch (const std::exception& e) {
                    LOG_FATAL(log::Arg{"fuurin::Timer"sv, "timer already canceled"sv},
                        log::Arg{"reason"sv, std::string(e.what())});
                }
            }
        };

        if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
                LOG_DEBUG(log::Arg{"fuurin::Timer"sv, "timer fired error"sv},
                    log::Arg{"reason"sv, ec.message()});
            }
            return;
        }

        if (timer->t.expires_at() == boost::asio::steady_timer::clock_type::time_point::min())
            return;

        // send notification
        timer->trigger->send(Part{uint8_t(1)});

        if (timer->singleshot)
            return;

        // rearm timer
        timer->t.expires_at(timer->t.expiry() + timer->interval);
        timer->t.async_wait(std::bind(&IOSteadyTimer::timerFired, timer, std::placeholders::_1));
        done = false;
    }

    /**
     * \brief Starts the timer after \c interval from now.
     * \param[in] timer Timer to work on.
     */
    static void timerStart(IOSteadyTimer* timer)
    {
        timer->t.expires_after(timer->interval);
        timer->t.async_wait(std::bind(&IOSteadyTimer::timerFired, timer, std::placeholders::_1));
    }

    /**
     * \brief Sets the expiry time to a well known invalid time.
     * In case the handler is still pending, then it will be called with operation_aborted error code.
     * In case the handler is already expired, the special value will be caught anyway.
     * \param[in] timer Timer to work on.
     * \param[in] ackPromise Promise to set to nofity command completion.
     */
    static void timerCancel(IOSteadyTimer* timer, std::promise<void>* ackPromise)
    {
        timer->t.expires_at(boost::asio::steady_timer::clock_type::time_point::min());
        ackPromise->set_value();
    }


private:
    /// Constructor.
    IOSteadyTimer(Context* const ctx, std::chrono::milliseconds interval, bool singleshot, Socket* trigger)
        : interval{interval}
        , singleshot{singleshot}
        , trigger{trigger}
        , t{boost::asio::steady_timer{ctx->ioContext()}}
        , cancelPromise{std::promise<bool>()}
    {
    }


private:
    const std::chrono::milliseconds interval; ///< \see Timer::interval_.
    const bool singleshot;                    ///< \see Timer::singleshot_.
    Socket* const trigger;                    ///< Socket to trigger.

    boost::asio::steady_timer t;      ///< ASIO timer.
    std::promise<bool> cancelPromise; ///< Promise set when timer is canceled.
};


Timer::Timer(Context* ctx, const std::string& name)
    : ctx_{ctx}
    , name_{name}
    , trigger_{std::make_unique<zmq::Socket>(ctx, zmq::Socket::PUSH)}
    , receiver_{std::make_unique<zmq::Socket>(ctx, zmq::Socket::PULL)}
    , interval_{0ms}
    , singleshot_{false}
{
    const std::string endpoint = "inproc://"s + name_;

    trigger_->setEndpoints({endpoint});
    receiver_->setEndpoints({endpoint});

    trigger_->setConflate(true);
    receiver_->setConflate(true);

    receiver_->bind();
    trigger_->connect();
}


Timer::~Timer() noexcept
{
    stop();
}


void* Timer::zmqPointer() const noexcept
{
    return receiver_->zmqPointer();
}


bool Timer::isOpen() const noexcept
{
    return true;
}


std::string Timer::description() const
{
    return name_;
}


void Timer::setInterval(std::chrono::milliseconds value) noexcept
{
    if (value < 0ms)
        value = 0ms;

    interval_ = value;
}


std::chrono::milliseconds Timer::interval() const noexcept
{
    return interval_;
}


void Timer::setSingleShot(bool value) noexcept
{
    singleshot_ = value;
}


bool Timer::isSingleShot() const noexcept
{
    return singleshot_;
}


void Timer::start()
{
    stop();

    auto tup = IOSteadyTimer::makeIOSteadyTimer(ctx_, interval_, singleshot_, trigger_.get());
    cancelFuture_ = std::move(std::get<0>(tup));
    timer_.reset(std::get<1>(tup));

    ctx_->ioContext().post(std::bind(&IOSteadyTimer::timerStart, timer_.get()));
}


void Timer::stop()
{
    if (!timer_)
        return;

    // send cancel command synchronously.
    std::promise<void> ackPromise;
    std::future<void> ackFuture = ackPromise.get_future();
    ctx_->ioContext().post(std::bind(&IOSteadyTimer::timerCancel, timer_.get(), &ackPromise));
    ackFuture.get();

    // wait for timer completion.
    cancelFuture_.get();

    timer_.reset();
}


void Timer::consume()
{
    Part p;
    receiver_->recv(&p);
}


bool Timer::isExpired() const
{
    Poller poll{PollerEvents::Type::Read, 0ms, const_cast<Timer*>(this)};
    return !poll.wait().empty();
}


bool Timer::isActive()
{
    return cancelFuture_.valid() &&
        cancelFuture_.wait_for(0ms) != std::future_status::ready;
}

} // namespace zmq
} // namespace fuurin
