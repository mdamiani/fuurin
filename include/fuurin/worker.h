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
#include "fuurin/workerconfig.h"
#include "fuurin/lrucache.h"
#include "fuurin/event.h"
#include "fuurin/topic.h"
#include "fuurin/uuid.h"

#include <memory>
#include <vector>
#include <tuple>


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
 * This class does not throw exceptions by itself.
 */
class Worker : public Runner
{
public:
    /**
     * \brief Initializes this worker.
     *
     * \param[in] id Identifier.
     * \param[in] initSequence Initial sequence number.
     */
    Worker(Uuid id = Uuid::createRandomUuid(), Topic::SeqN initSequence = 0);

    /**
     * \brief Destroys this worker.
     */
    virtual ~Worker() noexcept;

    /**
     * \brief Sets every topics to sync with and receive from broker.
     *
     * \see setTopicsNames(std::vector<Topic::Name>)
     */
    void setTopicsAll();

    /**
     * \brief Sets topics to sync with and receive from broker.
     *
     * If the list of names is empty, then none topic be received.
     *
     * \param[in] names List of topics names.
     *
     * \see setTopicsAll()
     */
    void setTopicsNames(const std::vector<Topic::Name>& names);

    /**
     * \brief Returns subscription topics.
     *
     * \return A tuple which first element is whether every topics
     *      were subscribed and the second element (otherwise)
     *      the specific topic names which were subscribed.
     *
     * \see setTopicsNames(std::vector<Topic::Name>)
     * \see setTopicsAll()
     */
    std::tuple<bool, std::vector<Topic::Name>> topicsNames() const;

    /**
     * \brief Sends data to the broker.
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

    /**
     * \return The last sequence number used for marking data.
     */
    Topic::SeqN seqNumber() const;


protected:
    /**
     * \brief Prepares configuration.
     *
     * \see Runner::prepareConfiguration()
     */
    virtual zmq::Part prepareConfiguration() const override;

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
            zmq::Context* zctx, zmq::Socket* zoper, zmq::Socket* zevent,
            zmq::Socket* zseqs);

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
         * \brief Save the configuration upon start.
         *
         * \param[in] part Configuration data.
         */
        void saveConfiguration(const zmq::Part& part);

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
         * \brief Accepts a topic, for the specified worker.
         *
         * If topic was accepted and its uuid corresponds to
         * this session's uuid, then current sequence number is
         * updated.
         *
         * \param[in] part Packed topic.
         *
         * \return Whether topic was accepted or not.
         *
         * \see acceptTopic(const Uuid&, Topic::SeqN)
         * \see notifySequenceNumber()
         */
        bool acceptTopic(const zmq::Part& part);

        /**
         * \brief Accepts a topic, for the specified worker.
         *
         * Topic is accepted only if its sequence number is
         * greater than the last value.
         *
         * \param[in] worker Worker uuid.
         * \param[in] value Sequence number.
         *
         * \return Whether topic was accepted or not.
         */
        bool acceptTopic(const Uuid& worker, Topic::SeqN value);

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

        /**
         * \brief Notifies for any change of current sequence number.
         */
        void notifySequenceNumber() const;


    protected:
        const std::unique_ptr<zmq::Socket> zsnapshot_; ///< ZMQ socket to receive snapshots.
        const std::unique_ptr<zmq::Socket> zdelivery_; ///< ZMQ socket to receive data.
        const std::unique_ptr<zmq::Socket> zdispatch_; ///< ZMQ socket to send data.
        const std::unique_ptr<ConnMachine> conn_;      ///< Connection state machine.
        const std::unique_ptr<SyncMachine> sync_;      ///< Connection sync machine.
        zmq::Socket* const zseqs_;                     ///< ZMQ socket to send sequence number.

        bool isOnline_;     ///< Whether the worker's connection is up.
        bool isSnapshot_;   ///< Whether for workers is syncing its snapshot.
        Uuid brokerUuid_;   ///< Broker which last sucessfully synced.
        WorkerConfig conf_; ///< Configuration for running the asynchronous task.

        /// Alias for worker's uuid type.
        using WorkerUuid = Uuid;

        Topic::SeqN seqNum_;                             ///< Sequence number.
        LRUCache<Topic::Name, bool> subscrTopic_;        ///< Subscribed topics.
        LRUCache<WorkerUuid, Topic::SeqN> workerSeqNum_; ///< Sequence numbers.
    };


protected:
    const std::unique_ptr<zmq::Socket> zseqs_; ///< ZMQ socket to send sequence number.
    const std::unique_ptr<zmq::Socket> zseqr_; ///< ZMQ socket to receive sequence number.

    mutable Topic::SeqN seqNum_; ///< Worker sequence number.

    bool subscrAll_;                       ///< Whether to subscribe to every topic.
    std::vector<Topic::Name> subscrNames_; ///< List of topic names.
};

} // namespace fuurin

#endif // FUURIN_WORKER_H
