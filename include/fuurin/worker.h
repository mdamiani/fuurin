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
#include <vector>
#include <tuple>
#include <optional>
#include <atomic>


namespace fuurin {
namespace zmq {
class Part;
} // namespace zmq


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
     * \param[in] name Description of this worker.
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
     * Any topic name shall be different than \ref SessionEnv::BrokerUpdt.
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
    std::tuple<bool, const std::vector<Topic::Name>&> topicsNames() const;

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
     * If worker is not \ref isRunning(), then message is silently discarded.
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
    ///@{
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
     * If worker is not \ref isRunning(), then synchronization request is silently discarded.
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
    ///@{
    Event waitForEvent(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    Event waitForEvent(zmq::Pollable& timeout) const;
    Event waitForEvent(zmq::Pollable* timeout) const;
    ///@}

    /**
     * \brief Waits for an event synchronously.
     *
     * This method is thread-safe.
     *
     * The event waited for depends on the actual performed action:
     *   - \ref start() => \ref Event::Type::Started.
     *   - \ref start() => \ref Event::Type::Online.
     *   - \ref stop() => \ref Event::Type::Stopped.
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
    ///@{
    bool waitForStarted(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    bool waitForStopped(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    bool waitForOnline(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    bool waitForOffline(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    std::optional<Topic> waitForTopic(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) const;
    ///@}

    using Runner::eventFD;

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
    virtual std::unique_ptr<Session> createSession() const override;

    /**
     * \brief Waits for specific events.
     *
     * \see Runner::waitForEvent(std::chrono::milliseconds, EventMatchFunc)
     */
    Event waitForEvent(std::chrono::milliseconds timeout, EventMatchFunc match) const;


protected:
protected:
    const std::unique_ptr<zmq::Socket> zseqs_; ///< ZMQ socket to send sequence number.
    const std::unique_ptr<zmq::Socket> zseqr_; ///< ZMQ socket to receive sequence number.

    mutable std::atomic<Topic::SeqN> seqNum_; ///< Worker sequence number.

    bool subscrAll_;                       ///< Whether to subscribe to every topic.
    std::vector<Topic::Name> subscrNames_; ///< List of topic names.
};

} // namespace fuurin

#endif // FUURIN_WORKER_H
