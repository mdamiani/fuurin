/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_RUNNER_H
#define FUURIN_RUNNER_H

#include "fuurin/event.h"
#include "fuurin/operation.h"
#include "fuurin/uuid.h"

#include <memory>
#include <future>
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <vector>


namespace fuurin {
class Elapser;

namespace zmq {
class Context;
class Socket;
class Part;
class Pollable;
class PollerWaiter;
} // namespace zmq


/**
 * \brief Runner is a base interface to run an asynchronous task in background.
 *
 * This class is not thread-safe.
 *
 * \see Broker
 * \see Worker
 */
class Runner
{
public:
    /**
     * \brief Initializes this runner.
     *
     * The core sockets for the inter-thread communication is setup,
     * using a runner specific endpoint. Both the sending and receiving
     * ends are created and connected/bound.
     *
     * \param[in] id Instance identifier.
     */
    Runner(Uuid id = Uuid::createRandomUuid());

    /**
     * \brief Stops this runner.
     *
     * \see stop()
     */
    virtual ~Runner() noexcept;

    /**
     * Disable copy.
     */
    ///{@
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;
    ///@}

    /**
     * \return Return the instance identifier.
     */
    Uuid uuid() const;

    /**
     * \brief Sets endpoints to connect to broker.
     *
     * If endpoints were not set, then IPC protocol will be used as default.
     *
     * \param[in] delivery List endpoints, default is {"ipc:///tmp/worker_delivery"}.
     * \param[in] dispatch List endpoints, default is {"ipc:///tmp/worker_dispatch"}.
     * \param[in] snapshot List endpoints, default is {"ipc:///tmp/broker_snapshot"}.
     *
     * \see endpointDelivery()
     * \see endpointDispatch()
     * \see endpointSnapshot()
     */
    void setEndpoints(const std::vector<std::string>& delivery,
        const std::vector<std::string>& dispatch,
        const std::vector<std::string>& snapshot);

    /**
     * \return Current endpoints.
     *
     * \see setEndpoints(std::vector<std::string>, std::vector<std::string>, std::vector<std::string>)
     */
    ///{@
    std::vector<std::string> endpointDelivery() const;
    std::vector<std::string> endpointDispatch() const;
    std::vector<std::string> endpointSnapshot() const;
    ///@}

    /**
     * \brief Starts the background thread of the main task.
     *
     * \return A future to hold the result (or an exception) of the computation,
     *      or and empty future in case the asynchronous task is already running.
     *
     * \exception Throws exceptions in case task could not be started.
     *
     * \see stop()
     * \see isRunning()
     * \see Session::run()
     * \see prepareConfiguration()
     */
    std::future<void> start();

    /**
     * \brief Stops the backgroud thread of the main task.
     *
     * This methods panics in case stop could not be performed,
     * i.e. an exception was thrown.
     *
     * \return \c true in case the task was running,
     *      \c false otherwise.
     *
     * \see start()
     * \see isRunning()
     * \see sendOperation(Operation::Type, zmq::Part&&)
     * \see Session::run()
     */
    bool stop() noexcept;

    /**
     * \return Whether the asynchronous task is running.
     *
     * This method is thread-safe.
     *
     * \see start()
     * \see stop()
     * \see Session::run()
     */
    bool isRunning() const noexcept;


protected:
    typedef uint8_t token_type_t; ///< Type of the execution token.

    ///< Function type to call when a sessions ends.
    using CompletionFunc = std::function<void()>;

    /**
     * \brief Sends an operation over a socket.
     *
     * \param[in] sock Socket the operation is sent to.
     * \param[in] token Operation token.
     * \param[in] oper Type of operation.
     * \param[in] payload Payload of operation.
     */
    static void sendOperation(zmq::Socket* sock, token_type_t token, Operation::Type oper, zmq::Part&& payload) noexcept;

    /**
     * \brief Receives an operation reply from a socket.
     *
     * \param[in] sock Socket the operation is received from.
     * \param[in] token Expected operation token.
     *
     * \return An operation object.
     */
    static Operation recvOperation(zmq::Socket* sock, token_type_t token) noexcept;


protected:
    class Session;

    /**
     * \return Runner's ZMQ Context.
     */
    zmq::Context* zmqCtx() const noexcept;

    /**
     * \brief Prepares configuration to send to the asynchronous task upon start.
     *
     * This method shall be overridden by subclasses in order to
     * return a specific configuration data.
     *
     * \return An empty \ref Part.
     *
     * \see start()
     */
    virtual zmq::Part prepareConfiguration() const;

    /**
     * \brief Instantiates a session which runs the asynchronous task.
     *
     * This method shall be overridden by subclasses in order to
     * instantiate a more specific session.
     *
     * \param[in] onComplete Function to call when session is completed.
     *                       It shall not throw exceptions.
     *
     * \return A pointer to a new session.
     *
     * \see Session
     * \see makeSession()
     */
    virtual std::unique_ptr<Session> createSession(CompletionFunc onComplete) const;

    /**
     * \brief Wrapper to initialize any specific session.
     *
     * \see createSession()
     */
    template<typename S, typename... Args>
    std::unique_ptr<Session> makeSession(CompletionFunc onComplete, Args&&... args) const
    {
        return std::make_unique<S>(uuid_, token_, onComplete, zctx_.get(), zopr_.get(), zevs_.get(),
            std::forward<Args>(args)...);
    }

    /**
     * \brief Sends an operation to perform to the asynchronous task.
     *
     * This method shall be called from the main thread.
     *
     * The operation is sent over the inter-thread communication socket.
     * Send of operation shall not fail, i.e. no exceptions must be thrown
     * by the inter-thread socket, otherwise a fatal error is raised.
     */
    ///{@
    void sendOperation(Operation::Type oper) noexcept;
    void sendOperation(Operation::Type oper, zmq::Part&& payload) noexcept;
    ///@}

    ///< Type used for function to receive an event;
    using EventRecvFunc = std::function<Event()>;

    /**
     * \brief Waits for events from the asynchronous task.
     *
     * This method shall be called from the main thread.
     * This method is thread-safe.
     *
     * \param[in] timeout Waiting deadline.
     *
     * \return The (optionally) received event.
     *
     * \see recvEvent()
     */
    Event waitForEvent(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));


private:
    friend class TestRunner;

    /**
     * \brief Waits for events from the asynchronous task.
     *
     * \param[in] poll Poller interface.
     * \param[in] dt Elapser interface.
     * \param[in] recv Function used to actually receive and event.
     * \param[in] timeout Waiting deadline.
     *
     * \return An \ref Event representing the (optionally) received event.
     *
     * \see waitForEvent(std::chrono::milliseconds)
     */
    static Event waitForEvent(zmq::PollerWaiter&& pw, Elapser&& dt, EventRecvFunc recv,
        std::chrono::milliseconds timeout);

    /**
     * \brief Receives an event notification from the asynchronous task.
     *
     * This method shall be called from the main thread.
     * The event is received from the inter-thread dish socket.
     *
     * In case the received event's token doesn't match the current one,
     * then the returned value is marked as invalid.
     *
     * \return An \ref Event with its type, the payload and whether it's valid or not.
     *
     * \see waitForEvent(std::chrono::milliseconds)
     */
    Event recvEvent();


protected:
    /**
     * \brief Session for the asynchronous task.
     *
     * This class holds the variables and logic which shall not shared between
     * the main and asynchronous task. Communication is establish using sockets
     * at creation time.
     *
     * Every spawned session has a \ref token_ representing the current execution task,
     * in order to filter out potential previous operations being read from the
     * inter-thread socket.
     *
     * \see Runner::start()
     */
    class Session
    {
    public:
        /**
         * \brief Creates a new session and initializes it.
         *
         * Passed sockets must be taken from a \ref Runner instance.
         *
         * \param[in] token Session token, it's constant as long as this session is alive.
         * \param[in] onComplete Function to call when session ends.
         * \param[in] zctx ZMQ context.
         * \param[in] zoper ZMQ socket to receive operation commands from main task.
         * \param[in] zevent ZMQ socket to send events to main task.
         */
        Session(Uuid id, token_type_t token, CompletionFunc onComplete,
            zmq::Context* zctx, zmq::Socket* zoper, zmq::Socket* zevent);

        /**
         * \brief Destructor.
         */
        virtual ~Session() noexcept;

        /**
         * Disable copy.
         */
        ///{@
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        ///@}

        /**
         * \brief Main body of the asynchronous task.
         *
         * This method represents the asynchrous task session,
         * so it will be running in a different thread than the main one.
         *
         * \exception Throws exceptions in case of unexpected errors.
         *
         * \see createPoller()
         * \see operationReady(Operation*)
         * \see socketReady(zmq::Pollable*)
         * \see recvOperation()
         */
        void run();


    protected:
        /**
         * \brief Creates the poller to be used in the main asynchronous task.
         *
         * Concrete classes shall override this virtual method to create a poller in order to
         * wait on the passed inter-thread \c sock and any other additional \ref Socket.
         *
         * This notification shall be executed in the asynchronous task thread.
         *
         * \remark Inter-thread socket \ref zopr_ shall be added to the list of sockets
         *      \ref zmq::PollerWaiter will wait for.
         *
         * \exception May throw exceptions.
         * \return A pointer to a newly created poller.
         */
        virtual std::unique_ptr<zmq::PollerWaiter> createPoller();

        /**
         * \brief Notifies whenever any operation request was received by the asynchrnous task.
         *
         * This notification shall be executed in the asynchronous task thread.
         *
         * Concrete classes shall override this virtual method in order to
         * handle and define any other specific tasks than \ref Operation::Stop.
         */
        virtual void operationReady(Operation* oper);

        /**
         * \brief Notifies whenever any item being polled is ready to be accessed by the asynchronous task.
         *
         * This notification shall be executed in the asynchronous task thread.
         *
         * Concrete classes shall override this virtual method in order to
         * handle their specific sockets. In case no additional sockets are
         * added by \ref createPoller() (other than the inter-thread \ref zopr_),
         * then this nofitication is never issued.
         *
         * \param[in] pble Ready pollable item.
         */
        virtual void socketReady(zmq::Pollable* pble);

        /**
         * \brief Sends an event notification to the main thread.
         *
         * This method shall be called from the asynchronous task thread.
         * The event is send over the inter-thread radio socket.
         *
         * \param[in] event The type of event to be notified.
         * \param[in] payload The payload of event to be notified.
         */
        void sendEvent(Event::Type event, zmq::Part&& payload);


    private:
        /**
         * \brief Receives an operation to perform from the main thread.
         *
         * This method shall be called from the asynchronous task thread.
         *
         * The operation is received from the inter-thread communication socket \ref zopr_.
         * Receive of operation shall not fail, i.e. no exceptions must be thrown
         * by the inter-thread socket, otherwise a fatal error is raised.
         *
         * In case the received operation's token doesn't match session \ref token_,
         * then the returned values are marked as invalid.
         *
         * \return An \ref Operation with its type, the payload and whether it's valid or not.
         *
         * \see run()
         */
        Operation recvOperation() noexcept;


    protected:
        const Uuid uuid_;              ///< Identifier.
        const token_type_t token_;     ///< Session token.
        const CompletionFunc docompl_; ///< Completion action.
        zmq::Context* const zctx_;     ///< \see Runner::zctx_.
        zmq::Socket* const zopr_;      ///< \see Runner::zopr_.
        zmq::Socket* const zevs_;      ///< \see Runner::zevs_.
    };


private:
    const Uuid uuid_;                          ///< Identifier.
    const std::unique_ptr<zmq::Context> zctx_; ///< ZMQ context.
    const std::unique_ptr<zmq::Socket> zops_;  ///< Inter-thread sending socket.
    const std::unique_ptr<zmq::Socket> zopr_;  ///< Inter-thread receiving socket.
    const std::unique_ptr<zmq::Socket> zevs_;  ///< Inter-thread events notifications.
    const std::unique_ptr<zmq::Socket> zevr_;  ///< Inter-thread events reception.

    std::atomic<bool> running_;       ///< Whether the task is running.
    std::atomic<token_type_t> token_; ///< Current execution token for the task.

    std::vector<std::string> endpDelivery_; ///< List of endpoints.
    std::vector<std::string> endpDispatch_; ///< List of endpoints.
    std::vector<std::string> endpSnapshot_; ///< List of endpoints.
};

} // namespace fuurin

#endif // FUURIN_RUNNER_H
