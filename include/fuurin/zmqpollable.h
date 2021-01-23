/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQPOLLABLE_H
#define ZMQPOLLABLE_H

#include <list>
#include <string>


namespace fuurin {
namespace zmq {

class PollerObserver;


/**
 * \brief Interface for items to be pollable by a \ref Poller object.
 *
 * This interface is mostly implmented by \ref Socket.
 */
class Pollable
{
public:
    /**
     * \brief Constructor.
     */
    Pollable();

    /**
     * \brief Destructor.
     */
    virtual ~Pollable() noexcept;

    /**
     * Disable copy.
     */
    ///@{
    Pollable(const Pollable&) = delete;
    Pollable& operator=(const Pollable&) = delete;
    ///@}

    /**
     * \return The underlying raw ZMQ pointer.
     */
    virtual void* zmqPointer() const noexcept = 0;

    /**
     * \return Whether this socket is open.
     */
    virtual bool isOpen() const noexcept = 0;

    /**
     * \return A description of this pollable socket.
     */
    virtual std::string description() const = 0;


public:
    // TODO: make observer inteface loose coupled and make remove these public methods.
    /**
     * \brief Registers/unregisters a poller observer for this pollable socket.
     *
     * This method is intended to be automatically called by a \ref PollerObserver
     * when it is passed a \ref Pollable to poll.
     *
     * \param[in] poller Poller of this socket.
     */
    ///@{
    void registerPoller(PollerObserver* poller);
    void unregisterPoller(PollerObserver* poller);
    ///@}

    /**
     * \return The list of registered pollers.
     */
    size_t pollersCount() const noexcept;


protected:
    std::list<PollerObserver*> observers_; ///< List of poller observers.
};

} // namespace zmq
} // namespace fuurin

#endif // ZMQPOLLABLE_H
