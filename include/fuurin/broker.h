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
    /**
     * \return A new broker session.
     * \see Runner::makeSession()
     * \see BrokerSession
     */
    virtual std::unique_ptr<Session> makeSession(std::function<void()> oncompl) const override;


protected:
    /**
     * \brief Broker specific asynchronous task session.
     * \see Runner::Session
     */
    class BrokerSession : public Session
    {
    public:
        /**
         * \brief Creates the broker asynchronous task session.
         *
         * The socket used to receive storage is created and bound.
         *
         * \see Runner::Session::Session(...)
         */
        BrokerSession(token_type_t token, std::function<void()> oncompl,
            const std::unique_ptr<zmq::Context>& zctx,
            const std::unique_ptr<zmq::Socket>& zoper,
            const std::unique_ptr<zmq::Socket>& zevents);

        /**
         * \brief Destructor.
         */
        virtual ~BrokerSession() noexcept;


    protected:
        /// \see Runner::Session::createPoller()
        virtual std::unique_ptr<zmq::PollerWaiter> createPoller() override;

        /// \see Runner::Session::socketReady(zmq::Pollable*)
        virtual void socketReady(zmq::Pollable* pble) override;


    protected:
        const std::unique_ptr<zmq::Socket> zstore_; ///< ZMQ socket to store data.
    };
};


} // namespace fuurin

#endif // FUURIN_BROKER_H
