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


namespace fuurin {
namespace zmq {

class Context;
class Socket;


/**
 * \brief Timer object which is \ref Pollable by a \ref Poller.
 *
 * The actual implemention of this object is made of two socket ends of type
 * \ref Socket::Type::PAIR using "inproc://" transport.
 *
 * When the timer is \ref trigger() 'ed by sending data to the writable end,
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
    ///{@
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


protected:
    /**
     * \brief Triggers this timer.
     * Any poller who is waiting for this timer will be woken up.
     */
    void trigger();


private:
    Context* const ctx_;     ///< ZMQ context of this timer.
    const std::string name_; ///< Timer description.

    std::unique_ptr<Socket> trigger_;  ///< Writable end of this timer.
    std::unique_ptr<Socket> receiver_; ///< Pollable end of this timer.
};

} // namespace zmq
} // namespace fuurin

#endif // ZMQTIMER_H
