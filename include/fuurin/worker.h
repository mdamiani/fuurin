/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_WORKER_H
#define FUURIN_WORKER_H

#include "fuurin/runner.h"

#include <memory>


namespace fuurin {


/**
 * \brief Worker is the client interface to implement a service.
 *
 * This class is not thread-safe.
 */
class Worker : public Runner
{
public:
    /**
     * \brief Initializes this worker.
     */
    Worker();

    /**
     * \brief Destroys this worker.
     */
    virtual ~Worker() noexcept;


    /**
     * \brief Stores data to the broker.
     *
     * \exception Error The worker task is not running.
     *
     * \see isRunning()
     */
    void store(zmq::Part&& data);


protected:
    /**
     * \brief Worker operations.
     */
    enum Operation : oper_type_t
    {
        Store = Runner::Operation::OPER_COUNT,
    };


protected:
    virtual std::unique_ptr<zmq::PollerWaiter> createPoller(zmq::Socket* sock) override;
    virtual void operationReady(oper_type_t oper, zmq::Part& payload) override;
    virtual void socketReady(zmq::Socket* sock) override;

public:
    std::tuple<zmq::Part, Runner::EventRead> waitForEvent(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));


protected:
    std::unique_ptr<zmq::Socket> zstore_; ///< ZMQ socket to store data.
};


} // namespace fuurin

#endif // FUURIN_WORKER_H
