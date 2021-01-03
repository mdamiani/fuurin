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
#include <optional>


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
    explicit Worker(Uuid id = Uuid::createRandomUuid(), Topic::SeqN initSequence = 0, const std::string& name = "worker");

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
     * \brief Sends a message to the broker(s).
     *
     * A message \ref Topic is first created and then dispatched to any connected broker(s).
     * Depending on subscriptions, i.e. \ref topicsNames(), an \ref Event may be received back.
     * If a message is marked as \ref Topic::State, then it will be retrieved back by \ref sync().
     *
     * The policy for messages which are sent before a worker is actually connected to broker(s),
     * that is before the \ref Event::Type::Online event, depends on the underlying ZMQ socket type.
     * Thus messages might be kept in socket's buffers.
     *
     * Sequence number is increased every time this function is called.
     *
     * If worker is not \ref Running(), then message is silently discarded.
     *
     * \param[in] name Name of topic, no more than \ref Topic::Name::capacity() chars.
     * \param[in] data Data of topic.
     * \param[in] type Type of topic.
     *
     * \see isRunning()
     * \see waitForEvent(std::chrono::milliseconds)
     * \see seqNumber()
     * \see sync()
     */
    ///{@
    void dispatch(Topic::Name name, const Topic::Data& data, Topic::Type type = Topic::State);
    void dispatch(Topic::Name name, Topic::Data& data, Topic::Type type = Topic::State);
    void dispatch(Topic::Name name, Topic::Data&& data, Topic::Type type = Topic::State);
    ///@}

    /**
     * \brief Sends a synchronization request to the broker.
     *
     * Every stored topic message with type \ref Topic::State will be received by
     * a synchronization operation.
     *
     * Synchronization topics will be received as \ref Event::Type::SyncElement elements,
     * after a \ref Event::Type::SyncBegin event.
     *
     * Received topics will match the subscribed ones.
     *
     * Sequence number might be also updated as part of synchronization.
     *
     * If worker is not \ref Running(), then synchronization request is silently discarded.
     *
     * \see isRunning()
     * \see waitForEvent(std::chrono::milliseconds)
     * \see topicNames()
     */
    void sync();

    /**
     * \brief Waits for events from the asynchronous task.
     *
     * This method is thread-safe.
     *
     * \param[in] timeout Waiting deadline, -1ms for infinite timeout.
     *
     * \return The received event, or \ref Event::Type::Invalid.
     */
    Event waitForEvent(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;

    /**
     * \brief Waits for an event synchronously.
     *
     * This method is thread-safe.
     *
     * The event waited for depends on the actual performed action:
     *   - \ref start() => \ref Event::Type::Start.
     *   - \ref start() => \ref Event::Type::Online.
     *   - \ref stop() => \ref Event::Type::Stop.
     *   - \ref stop() => \ref Event::Type::Offline.
     *   - \ref dispatch() => \ref Event::Type::Delivery.
     *   - \ref sync() => \ref Event::Type::SyncElement.
     *
     * Every other event, which is different from the requested one, will be discarded.
     *
     * \param[in] timeout Waiting deadline, -1ms for infinite timeout.
     *
     * \return \c false if event is not received within the specified timeout.
     *
     * \see waitForEvent(std::chrono::milliseconds)
     */
    ///{@
    bool waitForStarted(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    bool waitForStopped(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    bool waitForOnline(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    bool waitForOffline(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    std::optional<Topic> waitForTopic(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    ///@}

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

    /**
     * \brief Waits for specific events.
     *
     * \see Runner::waitForEvent(std::chrono::milliseconds, std::function<bool(Event::Type)>)
     */
    Event waitForEvent(std::chrono::milliseconds timeout,
        const std::function<bool(Event::Type)>& match) const;


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
        explicit WorkerSession(const std::string& name, Uuid id, token_type_t token,
            CompletionFunc onComplete, zmq::Context* zctx, zmq::Socket* zoper, zmq::Socket* zevent,
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
