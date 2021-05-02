/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "zmqiotimer.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "log.h"

#include <boost/scope_exit.hpp>

#include <functional>


using namespace std::literals::string_view_literals;


namespace fuurin {
namespace zmq {


IOSteadyTimer::IOSteadyTimer(Context* const ctx, std::chrono::milliseconds interval, bool singleshot,
    const Part& notif, Socket* trigger)
    : interval{interval}
    , singleshot{singleshot}
    , notif_{notif}
    , trigger{trigger}
    , t{boost::asio::steady_timer{ctx->ioContext()}}
    , cancelPromise{std::promise<bool>()}
{
}


std::tuple<std::future<bool>, IOSteadyTimer*> IOSteadyTimer::makeIOSteadyTimer(Context* const ctx,
    std::chrono::milliseconds interval, bool singleshot, const Part& notif, Socket* trigger)
{
    auto timer = new IOSteadyTimer(ctx, interval, singleshot, notif, trigger);
    return std::make_tuple(timer->cancelPromise.get_future(), timer);
}


void IOSteadyTimer::timerFired(IOSteadyTimer* timer, const boost::system::error_code& ec)
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
    timer->trigger->send(Part{timer->notif_});

    if (timer->singleshot)
        return;

    // rearm timer
    timer->t.expires_at(timer->t.expiry() + timer->interval);
    timer->t.async_wait(std::bind(&IOSteadyTimer::timerFired, timer, std::placeholders::_1));
    done = false;
}


void IOSteadyTimer::timerStart(IOSteadyTimer* timer)
{
    timer->t.expires_after(timer->interval);
    timer->t.async_wait(std::bind(&IOSteadyTimer::timerFired, timer, std::placeholders::_1));
}


void IOSteadyTimer::timerCancel(IOSteadyTimer* timer, std::promise<void>* ackPromise)
{
    timer->t.expires_at(boost::asio::steady_timer::clock_type::time_point::min());
    ackPromise->set_value();
}


void IOSteadyTimer::postTimerStart(Context* ctx, IOSteadyTimer* timer)
{
    ctx->ioContext().post(std::bind(&IOSteadyTimer::timerStart, timer));
}


void IOSteadyTimer::postTimerCancel(Context* ctx, IOSteadyTimer* timer)
{
    // send cancel command synchronously.
    std::promise<void> ackPromise;
    std::future<void> ackFuture = ackPromise.get_future();
    ctx->ioContext().post(std::bind(&IOSteadyTimer::timerCancel, timer, &ackPromise));
    ackFuture.get();
}

} // namespace zmq
} // namespace fuurin
