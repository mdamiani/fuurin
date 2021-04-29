/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQIOTIMER_H
#define ZMQIOTIMER_H

#include "fuurin/zmqpart.h"

#include <boost/asio/steady_timer.hpp>

#include <tuple>
#include <chrono>
#include <future>


namespace fuurin {
namespace zmq {

class Context;
class Socket;


/**
 * \brief Wrapper for ASIO steady timer.
 */
class IOSteadyTimer
{
public:
    /**
     * \brief Creates a new timer along with its completion future.
     */
    static std::tuple<std::future<bool>, IOSteadyTimer*> makeIOSteadyTimer(Context* const ctx,
        std::chrono::milliseconds interval, bool singleshot, const Part& notif, Socket* trigger);

    /**
     * \brief Timer expiration management.
     * \param[in] timer Timer to work on.
     * \param[in] ec Error code.
     */
    static void timerFired(IOSteadyTimer* timer, const boost::system::error_code& ec);

    /**
     * \brief Starts the timer after \c interval from now.
     * \param[in] timer Timer to work on.
     */
    static void timerStart(IOSteadyTimer* timer);

    /**
     * \brief Sets the expiry time to a well known invalid time.
     * In case the handler is still pending, then it will be called with operation_aborted error code.
     * In case the handler is already expired, the special value will be caught anyway.
     * \param[in] timer Timer to work on.
     * \param[in] ackPromise Promise to set to nofity command completion.
     */
    static void timerCancel(IOSteadyTimer* timer, std::promise<void>* ackPromise);

    /**
     * \brief Starts synchronously an \ref IOSteadyTimer.
     *
     * \see timerStart
     */
    static void postTimerStart(Context* ctx, IOSteadyTimer* timer);

    /**
     * \brief Stops synchronously an \ref IOSteadyTimer.
     *
     * It waits for \ref timerCancel to complete.
     *
     * \see timerCancel
     */
    static void postTimerCancel(Context* ctx, IOSteadyTimer* timer);


private:
    /// Constructor.
    IOSteadyTimer(Context* const ctx, std::chrono::milliseconds interval, bool singleshot, const Part& notif, Socket* trigger);


private:
    const std::chrono::milliseconds interval; ///< \see Timer::interval_.
    const bool singleshot;                    ///< \see Timer::singleshot_.
    const Part notif_;                        ///< Notification payload.
    Socket* const trigger;                    ///< Socket to trigger.

    boost::asio::steady_timer t;      ///< ASIO timer.
    std::promise<bool> cancelPromise; ///< Promise set when timer is canceled.
};

} // namespace zmq
} // namespace fuurin

#endif // ZMQIOTIMER_H
