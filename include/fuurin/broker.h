/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_BROKER_H
#define FUURIN_BROKER_H

#include "fuurin/runner.h"

#include <memory>


namespace fuurin {


/**
 * \brief Broker is the server interface to implement a storage broker.
 *
 * This class is not thread-safe.
 */
class Broker : public Runner
{
public:
    /**
     * \brief Initializes this broker.
     */
    Broker();

    /**
     * \brief Destroys this broker.
     */
    virtual ~Broker() noexcept;


protected:
    virtual std::unique_ptr<zmq::PollerWaiter> createPoller(zmq::Socket* sock) override;
    virtual void socketReady(zmq::Socket* sock) override;


protected:
    std::unique_ptr<zmq::Socket> zstore_; ///< ZMQ socket to store data.
};


} // namespace fuurin

#endif // FUURIN_BROKER_H
