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
#include <string_view>


namespace fuurin {
namespace zmq {
class Part;
} // namespace zmq

class ConnMachine;


/**
 * \brief Worker is the client interface to implement a service.
 *
 * This class is not thread-safe.
 */
class Worker : public Runner
{
public:
    /**
     * \brief Type of event payload.
     *
     * \see Runner::EventType
     * \see waitForEvent(std::chrono::milliseconds)
     */
    enum struct EventType : event_type_t
    {
        /// Event for disconnection.
        Offline = event_type_t(Runner::EventType::COUNT),
        Online, ///< Event for connection.
        Stored, ///< Event delivered when \ref Worker::store(zmq::Part&&) was acknowledged.

        COUNT, ///< Number of events.
    };

    /**
     * \return Returns a representable description of the event.
     * \param[in] ev Event to print.
     */
    static std::string_view eventToString(event_type_t ev);


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

    /**
     * \see Runner::waitForEvent(std::chrono::milliseconds)
     */
    std::tuple<event_type_t, zmq::Part, Runner::EventRead> waitForEvent(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));


protected:
    /**
     * \brief Worker operations.
     */
    enum struct Operation : oper_type_t
    {
        /// Operation for storage.
        Store = oper_type_t(Runner::Operation::COUNT),
    };


protected:
    /**
     * \return A new worker session.
     * \see Runner::makeSession()
     * \see WorkerSession
     */
    virtual std::unique_ptr<Session> makeSession(std::function<void()> oncompl) const override;


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
         * The socket used to store data is created and connected.
         *
         * \see Runner::Session::Session(...)
         */
        WorkerSession(token_type_t token, std::function<void()> oncompl,
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

        /// \see Runner::Session::operationReady(oper_type_t, zmq::Part&&)
        virtual void operationReady(oper_type_t oper, zmq::Part&& payload) override;

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
         * \brief Sends announce message to the remote party.
         */
        void sendAnnounce();

        /**
         * \brief Collects a message which was published by a broker.
         *
         * \param[in] payload Message payload.
         */
        void collectBrokerMessage(zmq::Part&& payload);

        /**
         * \brief Notifies for any change of connection state.
         *
         * \param[in] isUp Whether the connection is up or down.
         */
        void notifyConnectionUpdate(bool isUp);


    protected:
        const std::unique_ptr<zmq::Socket> zstore_;   ///< ZMQ socket to store data.
        const std::unique_ptr<zmq::Socket> zcollect_; ///< ZMQ socket to collect data.
        const std::unique_ptr<zmq::Socket> zdeliver_; ///< ZMQ socket to deliver data.
        const std::unique_ptr<ConnMachine> conn_;     ///< Connection state machine.

        bool isOnline_; ///< Whether the worker's connection is up.
    };
};

} // namespace fuurin

#endif // FUURIN_WORKER_H
