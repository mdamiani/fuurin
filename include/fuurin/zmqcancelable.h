/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQCANCELABLE
#define ZMQCANCELABLE

#include "fuurin/zmqpollable.h"

#include <memory>
#include <chrono>


namespace fuurin {
namespace zmq {

class Context;
class Timer;


/**
 * \brief Cancelable is an interface used to stop a blocking API.
 *
 * This object is \ref Pollable by a \ref Poller. It acts like a
 * \ref Timer which expires when the canceling action is performed.
 *
 * Once the object is canceled/expired, there is no way to reset it,
 * so it is continuously readable by a poller.
 */
class Cancelable : public Pollable
{
public:
    explicit Cancelable(Context* ctx, const std::string& name);

    /**
     * \brief Destroys this cancelable.
     */
    virtual ~Cancelable() noexcept;

    /**
     * Disable copy.
     */
    ///@{
    Cancelable(const Cancelable&) = delete;
    Cancelable& operator=(const Cancelable&) = delete;
    ///@}

    /**
     * \return Pollable ZMQ socket.
     * \see Timer::zmqPointer()
     */
    virtual void* zmqPointer() const noexcept override;

    /**
     * \return Always \c true.
     * \see Timer::isOpen()
     */
    virtual bool isOpen() const noexcept override;

    /**
     * \return The passed endpoint.
     * \see Timer::description()
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
    Cancelable& withDeadline(std::chrono::milliseconds timeout);

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
    bool isCanceled();


private:
    std::unique_ptr<Timer> timer_;
};

} // namespace zmq
} // namespace fuurin


#endif // ZMQCANCELABLE
