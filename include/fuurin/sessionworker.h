/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_SESSIONWORKER_H
#define FUURIN_SESSIONWORKER_H

#include "fuurin/session.h"
#include "fuurin/lrucache.h"
#include "fuurin/workerconfig.h"
#include "fuurin/topic.h"


namespace fuurin {

class ConnMachine;
class SyncMachine;


/**
 * \brief Worker specific asynchronous task session.
 * \see Session
 */
class WorkerSession : public Session
{
public:
    /**
     * \brief Creates the worker asynchronous task session.
     *
     * The sockets used for communication are created.
     *
     * \see Session::Session(...)
     */
    explicit WorkerSession(const std::string& name, Uuid id, SessionEnv::token_t token,
        zmq::Context* zctx, zmq::Socket* zfin, zmq::Socket* zoper, zmq::Socket* zevent,
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
    /// \see Session::createPoller()
    virtual std::unique_ptr<zmq::PollerWaiter> createPoller() override;

    /// \see Session::operationReady(Operation)
    virtual void operationReady(Operation* oper) override;

    /// \see Session::socketReady(zmq::Pollable*)
    virtual void socketReady(zmq::Pollable* pble) override;


protected:
    /**
     * \brief Closes/Opens the sockets connection.
     */
    ///@{
    void connClose();
    void connOpen();
    ///@}

    /**
     * \brief Closes/Opens the sockets for synchronization.
     */
    ///@{
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
     * \brief To be called whenever state of connection changes.
     *
     * \param[in] newState New connection state, that is \ref ConnMachine::State.
     *
     * \see notifyConnectionUpdate(bool)
     */
    void onConnChanged(int newState);

    /**
     * \brief To be called whenever state of synchronization changes.
     *
     * \param[in] newState New synchronization state, that is \ref SyncMachine::State.
     *
     * \see notifySnapshotDownload(bool)
     */
    void onSyncChanged(int newState);

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

} // namespace fuurin


#endif // FUURIN_SESSIONWORKER_H
