/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQTIMER_H
#define ZMQTIMER_H

#include "fuurin/zmqpollable.h"

#include <memory>
#include <string>
#include <chrono>
#include <future>


namespace fuurin {
namespace zmq {

class Context;
class Socket;
class IOSteadyTimer;


/**
 * \brief Timer object which is \ref Pollable by a \ref PollerWaiter.
 *
 * The actual implemention of this object is made of two socket ends of type
 * \ref Socket::PAIR, using \c inproc:// transport.
 *
 * When the timer is triggered by sending data to the writable end,
 * then the pollable end becomes ready for read and any waiting poller
 * will be waken up.
 */
class Timer : public Pollable
{
public:
    /**
     * \brief Initializes this timer.
     * \param[in] ctx A valid ZMQ context.
     * \param[in] name Timer name. It must respect the rules for addressing
     *      transport \c zmq_inproc, so it must be unique in the same \ref Context.
     * \exception ZMQTimerCreateFailed Timer could not be created.
     */
    explicit Timer(Context* ctx, const std::string& name);

    /**
     * \brief Destroys this timer.
     */
    virtual ~Timer() noexcept;

    /**
     * Disable copy.
     */
    ///@{
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    ///@}

    /**
     * \return The underlying raw ZMQ pointer of \ref receiver_.
     */
    virtual void* zmqPointer() const noexcept override;

    /**
     * \return Always \c true.
     */
    virtual bool isOpen() const noexcept override;

    /**
     * \return The passed timer endpoint.
     */
    virtual std::string description() const override;

    /**
     * \brief Sets the expiration interval.
     * \param[in] value Interval. A negative value will cause a 0 interval to be set.
     * \see interval()
     */
    void setInterval(std::chrono::milliseconds value) noexcept;

    /**
     * \return The expiration interval.
     * \see setInterval(std::chrono::milliseconds)
     */
    std::chrono::milliseconds interval() const noexcept;

    /**
     * \brief Sets whether this timer is periodic or single shot.
     * Default value is \c false, so the timer is periodic.
     * \param[in] value \c true for single shot timer.
     * \see isSingleShot()
     */
    void setSingleShot(bool value) noexcept;

    /**
     * \return Whether the timer is single shot.
     * \see setSingleShot(bool)
     */
    bool isSingleShot() const noexcept;

    /**
     * \brief Starts or restarts the timer.
     * The timer will be scheduled to fire after \c interval() milliseconds.
     * Every change to timer properties, like \ref interval() and \ref isSingleShot(),
     * won't take effect until the next restart.
     * \see stop()
     * \see setInterval(std::chrono::milliseconds)
     * \see setSingleShot(bool)
     */
    void start();

    /**
     * \brief Cancels the timer.
     * \see start()
     */
    void stop();

    /**
     * \brief Consumes this timer once expired.
     * In case the timer has not expired, it waits until its expiration.
     * \see isExpired()
     */
    void consume();

    /**
     * \brief Check whether this timer has expired.
     * A call to \ref consume() will reset the expiration state.
     * \return \c true when the timer has expired.
     * \see consume()
     */
    bool isExpired() const;

    /**
     * \brief Tells whether a timer is active.
     * A periodic timer is active since it's \ref start()'ed until it is \ref stop()'ed.
     * A \ref isSingleShot() timer automatically becomes inactive once it expires.
     * \return Whether the timer is active.
     * \see start()
     * \see stop()
     * \see setSingleShot(bool)
     */
    bool isActive();


private:
    Context* const ctx_;     ///< ZMQ context of this timer.
    const std::string name_; ///< Timer description.

    const std::unique_ptr<Socket> trigger_;  ///< Writable end of this timer.
    const std::unique_ptr<Socket> receiver_; ///< Pollable end of this timer.

    std::unique_ptr<IOSteadyTimer> timer_; ///< ASIO timer.
    std::future<bool> cancelFuture_;       ///< Future to wait for the ASIO timer to cancel.

    std::chrono::milliseconds interval_; ///< Timer expiration interval.
    bool singleshot_;                    ///< Whether this time is single shot or not.
};

} // namespace zmq
} // namespace fuurin

#endif // ZMQTIMER_H
