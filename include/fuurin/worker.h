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
#include "fuurin/event.h"
#include "fuurin/topic.h"
#include "fuurin/uuid.h"

#include <memory>
#include <string_view>


namespace fuurin {
namespace zmq {
class Part;
} // namespace zmq

class ConnMachine;
class SyncMachine;


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
    Worker(Uuid id = Uuid::createRandomUuid());

    /**
     * \brief Destroys this worker.
     */
    virtual ~Worker() noexcept;

    /**
     * \brief Sends data to the broker.
     *
     * \exception Error The worker task is not running.
     *
     * \see isRunning()
     */
    ///{@
    void dispatch(Topic::Name name, const Topic::Data& data);
    void dispatch(Topic::Name name, Topic::Data& data);
    void dispatch(Topic::Name name, Topic::Data&& data);
    ///@}

    /**
     * \brief Sends a synchronization request to the broker.
     *
     * \see isRunning()
     */
    void sync();

    /**
     * \see Runner::waitForEvent(std::chrono::milliseconds)
     */
    Event waitForEvent(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));


protected:
    /**
     * \return A new worker session.
     * \see Runner::createSession()
     * \see Runner::makeSession()
     * \see WorkerSession
     */
    virtual std::unique_ptr<Session> createSession(CompletionFunc onComplete) const override;


protected:
    /**
     * \brief Worker specific asynchronous task session.
     * \see Runner::Session
     */
    class WorkerSession : public Session
    {
    public:
        /**
         * \brief Creates the worker asynchronous task session.
         *
         * The sockets used for communication are created.
         *
         * \see Runner::Session::Session(...)
         */
        WorkerSession(Uuid id, token_type_t token, CompletionFunc onComplete,
            const std::unique_ptr<zmq::Context>& zctx,
            const std::unique_ptr<zmq::Socket>& zoper,
            const std::unique_ptr<zmq::Socket>& zevent);

        /**
         * \brief Destructor.
         */
        virtual ~WorkerSession() noexcept;

        /**
         * \return Returns \c true when it's online.
         */
        bool isOnline() const noexcept;


    protected:
        /// \see Runner::Session::createPoller()
        virtual std::unique_ptr<zmq::PollerWaiter> createPoller() override;

        /// \see Runner::Session::operationReady(Operation)
        virtual void operationReady(Operation* oper) override;

        /// \see Runner::Session::socketReady(zmq::Pollable*)
        virtual void socketReady(zmq::Pollable* pble) override;


    protected:
        /**
         * \brief Closes/Opens the sockets connection.
         */
        ///{@
        void connClose();
        void connOpen();
        ///@}

        /**
         * \brief Closes/Opens the sockets for synchronization.
         */
        ///{@
        void snapClose();
        void snapOpen();
        ///@}

        /**
         * \brief Sends announce message to the remote party.
         */
        void sendAnnounce();

        /**
         * \brief Sends sync message to the remote party.
         */
        void sendSync(uint8_t seqn);

        /**
         * \brief Collects a message which was published by a broker.
         *
         * \param[in] payload Message payload.
         */
        void collectBrokerMessage(zmq::Part&& payload);

        /**
         * \brief Receives snapshot data from broker.
         *
         * \param[in] payload Payload received from broker.
         */
        void recvBrokerSnapshot(zmq::Part&& payload);

        /**
         * \brief Notifies for any change of connection state.
         *
         * \param[in] isUp Whether the connection is up or down.
         */
        void notifyConnectionUpdate(bool isUp);

        /**
         * \brief Notifies for any change of snapshot download.
         *
         * \param[in] isSync Whether the snaphost is being synced.
         */
        void notifySnapshotDownload(bool isSync);


    protected:
        const std::unique_ptr<zmq::Socket> zsnapshot_; ///< ZMQ socket to receive snapshots.
        const std::unique_ptr<zmq::Socket> zdelivery_; ///< ZMQ socket to receive data.
        const std::unique_ptr<zmq::Socket> zdispatch_; ///< ZMQ socket to send data.
        const std::unique_ptr<ConnMachine> conn_;      ///< Connection state machine.
        const std::unique_ptr<SyncMachine> sync_;      ///< Connection sync machine.

        bool isOnline_;   ///< Whether the worker's connection is up.
        bool isSnapshot_; ///< Whether for workers is syncing its snapshot.
    };
};

} // namespace fuurin

#endif // FUURIN_WORKER_H
