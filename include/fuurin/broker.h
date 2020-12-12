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
#include "fuurin/brokerconfig.h"
#include "fuurin/topic.h"
#include "fuurin/uuid.h"
#include "fuurin/lrucache.h"

#include <memory>
#include <list>


namespace fuurin {

namespace zmq {
class Timer;
} // namespace zmq


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
    Broker(Uuid id = Uuid::createRandomUuid());

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
    virtual std::unique_ptr<Session> createSession(CompletionFunc onComplete) const override;


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
        BrokerSession(Uuid id, token_type_t token, CompletionFunc onComplete,
            zmq::Context* zctx, zmq::Socket* zoper, zmq::Socket* zevents);

        /**
         * \brief Destructor.
         */
        virtual ~BrokerSession() noexcept;


    protected:
        /// \see Runner::Session::createPoller()
        virtual std::unique_ptr<zmq::PollerWaiter> createPoller() override;

        /// \see Runner::Session::operationReady(Operation*)
        virtual void operationReady(Operation* oper) override;

        /// \see Runner::Session::socketReady(zmq::Pollable*)
        virtual void socketReady(zmq::Pollable* pble) override;


    protected:
        /**
         * \brief Save the configuration upon start.
         *
         * \param[in] part Configuration data.
         */
        void saveConfiguration(const zmq::Part& part);

        /**
         * \brief Binds sockets.
         *
         * Endpoints are contained in broker's \ref conf_.
         *
         * \see saveConfiguration(zmq::Part).
         */
        void openSockets();

        /**
         * \brief Close sockets.
         */
        void closeSockets();

        /**
         * \brief Sends a keepalive.
         */
        void sendHugz();

        /**
         * \brief Collects a message which was published by a worker.
         *
         * \param[in] payload Message payload.
         */
        void collectWorkerMessage(zmq::Part&& payload);

        /**
         * \brief Stores a topic into local storage.
         *
         * This functions checks the topic's sequence number and
         * it discards it if it's lower than the last sequence number,
         * for any source worker.
         *
         * \param[in] t Topic to store.
         *
         * \return \c false in case topic is discarded, i.e. not stored.
         */
        bool storeTopic(const Topic& t);

        /**
         * \brief Receives a synchronous command requested by a worker.
         *
         * \param[in] payload Message payload.
         */
        void receiveWorkerCommand(zmq::Part&& payload);

        /**
         * \brief Replies with the current snapshot.
         *
         * \param[in] rouID Requester's routing ID.
         * \param[in] seqn Request sequence number.
         * \param[in] params Snaphost synchronization parameters.
         */
        void replySnapshot(uint32_t rouID, uint8_t seqn, zmq::Part&& params);


    protected:
        const std::unique_ptr<zmq::Socket> zsnapshot_; ///< ZMQ socket send snapshots.
        const std::unique_ptr<zmq::Socket> zdelivery_; ///< ZMQ socket receive data.
        const std::unique_ptr<zmq::Socket> zdispatch_; ///< ZMQ socket send data.
        const std::unique_ptr<zmq::Timer> zhugz_;      ///< ZMQ timer to send keepalives.

        BrokerConfig conf_; ///< Session configuration.

        /// Alias for worker's uuid type.
        using WorkerUuid = Uuid;

        LRUCache<Topic::Name, LRUCache<WorkerUuid, Topic>> storTopic_; ///< Topic storage.
        LRUCache<WorkerUuid, Topic::SeqN> storWorker_;                 ///< Worker storage.
    };
};


} // namespace fuurin

#endif // FUURIN_BROKER_H
