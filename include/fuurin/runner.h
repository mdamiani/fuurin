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

#include <memory>
#include <future>
#include <atomic>
#include <tuple>
#include <chrono>
#include <functional>


namespace fuurin {

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
     * \brief Type of event read.
     *
     * \see waitForEvent(std::chrono::milliseconds)
     * \see sendEvent(event_type_t, zmq::Part&&)
     */
    enum struct EventRead
    {
        Timeout, ///< The read operation timed out.
        Success, ///< The read operation successfully returned an event.
        Discard, ///< The read operation returned an old event.
    };

    typedef uint8_t event_type_t; ///< Type of the event kind.

    /**
     * \brief Type of event payload.
     *
     * \see waitForEvent(std::chrono::milliseconds)
     * \see sendEvent(event_type_t, zmq::Part&&)
     */
    enum struct EventType : event_type_t
    {
        Invalid, ///< Event is invalid.
        Started, ///< Event delivered when \ref Runner::start() was acknowledged.
        Stopped, ///< Event delivered when \ref Runner::stop() was acknowledged.

        COUNT, ///< Number of events.
    };


public:
    /**
     * \brief Initializes this runner.
     *
     * The core sockets for the inter-thread communication is setup,
     * using a runner specific endpoint. Both the sending and receiving
     * ends are created and connected/bound.
     */
    Runner();

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
     * \see sendOperation(oper_type_t)
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
    typedef uint8_t oper_type_t;  ///< Type of operation.

    /**
     * \brief Type of operation to issue to the asynchronous task.
     *
     * \see sendOperation(Operation)
     * \see recvOperation()
     * \see operationReady(oper_type_t, zmq::Part&&)
     */
    enum struct Operation : oper_type_t
    {
        Invalid, ///< Invalid command.
        Start,   ///< Request to start the asynchronous task.
        Stop,    ///< Request to stop the asynchronous task.

        COUNT, ///< Number of operations.
    };

    ///< Function type to call when a sessions ends.
    using CompletionFunc = std::function<void()>;


protected:
    class Session;

    /**
     * \brief Instantiates a session which runs the asynchronous task.
     *
     * This method shall be overridden by subclasses in order to
     * instantiate a more specific session.
     *
     * \param[in] onComplete Function to call when session is completed.
     *                       It shall not throw exceptions.
     *
     * \see Session
     * \return A pointer to a new session.
     *
     * \see makeSession()
     */
    virtual std::unique_ptr<Session> createSession(CompletionFunc onComplete) const;

    /**
     * \brief Wrapper to initialize any specific session.
     *
     * \see createSession()
     */
    template<typename S>
    std::unique_ptr<Session> makeSession(CompletionFunc onComplete) const
    {
        return std::make_unique<S>(token_, onComplete, zctx_, zopr_, zevs_);
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
    void sendOperation(oper_type_t oper) noexcept;
    void sendOperation(oper_type_t oper, zmq::Part&& payload) noexcept;
    ///@}

    /**
     * \brief Waits for events from the asynchronous task.
     *
     * This method shall be called from the main thread.
     * This method is thread-safe.
     *
     * \return The (optionally) received event.
     *
     * \see recvEvent()
     */
    std::tuple<event_type_t, zmq::Part, Runner::EventRead> waitForEvent(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));


private:
    /**
     * \brief Receives an event notification from the asynchronous task.
     *
     * This method shall be called from the main thread.
     * The event is received from the inter-thread dish socket.
     *
     * In case the received event's token doesn't match the current one,
     * then the returned value is marked as invalid.
     *
     * \return A tuple with the event and whether it's valid or not.
     *
     * \see waitForEvent(std::chrono::milliseconds)
     */
    std::tuple<event_type_t, zmq::Part, bool> recvEvent();


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
        Session(token_type_t token, CompletionFunc onComplete,
            const std::unique_ptr<zmq::Context>& zctx,
            const std::unique_ptr<zmq::Socket>& zoper,
            const std::unique_ptr<zmq::Socket>& zevent);

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
         * \see operationReady(oper_type_t, zmq::Part&&)
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
        virtual void operationReady(oper_type_t oper, zmq::Part&& payload);

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
        void sendEvent(event_type_t event, zmq::Part&& payload);


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
         * \return A tuple with the type of operation, the payload and
         *      whether they are valid or not.
         *
         * \see run()
         */
        std::tuple<oper_type_t, zmq::Part, bool> recvOperation() noexcept;


    protected:
        const token_type_t token_;                  ///< Session token.
        const CompletionFunc docompl_;              ///< Completion action.
        const std::unique_ptr<zmq::Context>& zctx_; ///< \see Runner::zctx_.
        const std::unique_ptr<zmq::Socket>& zopr_;  ///< \see Runner::zopr_.
        const std::unique_ptr<zmq::Socket>& zevs_;  ///< \see Runner::zevs_.
    };


private:
    const std::unique_ptr<zmq::Context> zctx_; ///< ZMQ context.
    const std::unique_ptr<zmq::Socket> zops_;  ///< Inter-thread sending socket.
    const std::unique_ptr<zmq::Socket> zopr_;  ///< Inter-thread receiving socket.
    const std::unique_ptr<zmq::Socket> zevs_;  ///< Inter-thread events notifications.
    const std::unique_ptr<zmq::Socket> zevr_;  ///< Inter-thread events reception.
    std::atomic<bool> running_;                ///< Whether the task is running.
    std::atomic<token_type_t> token_;          ///< Current execution token for the task.
};                                             // namespace fuurin

} // namespace fuurin

#endif // FUURIN_RUNNER_H
