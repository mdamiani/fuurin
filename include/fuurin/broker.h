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
#include "fuurin/uuid.h"

#include <memory>
#include <string>


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
    explicit Broker(Uuid id = Uuid::createRandomUuid(), const std::string& name = "broker");

    /**
     * \brief Destroys this broker.
     */
    virtual ~Broker() noexcept;


protected:
    /**
     * \brief Prepares configuration.
     *
     * \see Runner::prepareConfiguration()
     */
    virtual zmq::Part prepareConfiguration() const override;

    /**
     * \return A new broker session.
     * \see Runner::createSession()
     * \see Runner::makeSession()
     * \see BrokerSession
     */
    virtual std::unique_ptr<Session> createSession() const override;
};

} // namespace fuurin

#endif // FUURIN_BROKER_H
