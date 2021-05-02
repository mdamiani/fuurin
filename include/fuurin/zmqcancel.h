/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQCANCEL_H
#define ZMQCANCEL_H

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
 * \brief Cancellation is an interface used to stop a blocking API.
 *
 * This object is \ref Pollable by a \ref PollerWaiter. It acts like a
 * \ref Timer which expires when the canceling action is performed.
 *
 * Once the object is canceled/expired, there is no way to reset it,
 * so it is continuously readable by a poller.
 *
 * Internally it uses an "inproc" ZMQ transport.
 *
 * This object is not thread-safe, but polling from multiple \ref PollerWaiter
 * objects is thread-safe.
 */
class Cancellation : public Pollable
{
public:
    explicit Cancellation(Context* ctx, const std::string& name);

    /**
     * \brief Destroys this cancelable.
     */
    virtual ~Cancellation() noexcept;

    /**
     * Disable copy.
     */
    ///@{
    Cancellation(const Cancellation&) = delete;
    Cancellation& operator=(const Cancellation&) = delete;
    ///@}

    /**
     * This method is thread-safe.
     *
     * It can be used by multiple \ref PollerWaiter objects.
     *
     * \return Pollable ZMQ socket.
     */
    virtual void* zmqPointer() const noexcept override;

    /**
     * \return Always \c true.
     */
    virtual bool isOpen() const noexcept override;

    /**
     * \return Fixed description.
     */
    virtual std::string description() const override;

    /**
     * \brief Set a cancellation deadline.
     *
     * \param[in] timeout Expiration timeout.
     *
     * \return Reference to this object.
     *
     * \see setDeadline(std::chrono::milliseconds)
     */
    Cancellation& withDeadline(std::chrono::milliseconds timeout);

    /**
     * \brief Set a cancellation deadline.
     *
     * In case a negative \c timeout is passed,
     * then no cancellation will be programmed.
     *
     * \param[in] timeout Expiration timeout.
     *
     * \see withDeadline(std::chrono::milliseconds)
     */
    void setDeadline(std::chrono::milliseconds timeout);

    /**
     * \return Cancellation programmed deadline.
     */
    std::chrono::milliseconds deadline() const noexcept;

    /**
     * \brief Triggers the cancellation.
     *
     * Once this object is canceled, then it becomes
     * readable by a \ref PollerWaiter.
     */
    void cancel();

    /**
     * \brief Whether this object was canceled.
     *
     * \return \c true When canceled.
     *
     * \see cancel()
     */
    bool isCanceled() const;


protected:
    /**
     * \brief Starts or restarts the cancellation.
     *
     * \param[in] timeout Expiration timeout.
     *
     * \see stop()
     */
    void start(std::chrono::milliseconds timeout);

    /**
     * \brief Stops the cancellation.
     *
     * \see start()
     */
    void stop();


private:
    Context* const ctx_;     ///< ZMQ context of this object.
    const std::string name_; ///< Cancellation description.

    const std::unique_ptr<Socket> trigger_;  ///< Writable end of this object.
    const std::unique_ptr<Socket> receiver_; ///< Pollable end of this object.

    std::unique_ptr<IOSteadyTimer> timer_; ///< ASIO timer.
    std::future<bool> cancelFuture_;       ///< Future to wait for the ASIO timer to cancel.

    std::chrono::milliseconds deadline_; ///< Expiration deadline.
};

} // namespace zmq
} // namespace fuurin


#endif // ZMQCANCEL_H
