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

#include "fuurin/sessionenv.h"
#include "fuurin/event.h"
#include "fuurin/operation.h"
#include "fuurin/uuid.h"

#include <memory>
#include <future>
#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include <string>
#include <string_view>
#include <atomic>


namespace fuurin {
class Session;

namespace zmq {
class Context;
class Socket;
class Part;
template<typename...>
class Poller;
class Pollable;
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
     * \param[in] name Description of this runner.
     */
    explicit Runner(Uuid id = Uuid::createRandomUuid(), const std::string& name = "runner");

    /**
     * \brief Stops this runner.
     *
     * \see stop()
     */
    virtual ~Runner() noexcept;

    /**
     * \return Runner's ZMQ Context.
     */
    zmq::Context* context() const noexcept;

    /**
     * Disable copy.
     */
    ///@{
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;
    ///@}

    /**
     * \return Worker's name.
     */
    std::string_view name() const;

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
    ///@{
    const std::vector<std::string>& endpointDelivery() const;
    const std::vector<std::string>& endpointDispatch() const;
    const std::vector<std::string>& endpointSnapshot() const;
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
    /**
     * \brief Sends an operation over a socket.
     *
     * \param[in] sock Socket the operation is sent to.
     * \param[in] token Operation token.
     * \param[in] oper Type of operation.
     * \param[in] payload Payload of operation.
     */
    static void sendOperation(zmq::Socket* sock, SessionEnv::token_t token, Operation::Type oper, zmq::Part&& payload) noexcept;


protected:
    /**
     * \brief Prepares configuration to send to the asynchronous task upon start.
     *
     * This method shall be overridden by subclasses in order to
     * return a specific configuration data.
     *
     * \return An empty \ref zmq::Part.
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
     * \return A pointer to a new session.
     *
     * \see Session
     * \see makeSession()
     */
    virtual std::unique_ptr<Session> createSession() const;

    /**
     * \brief Wrapper to initialize any specific session.
     *
     * \see createSession()
     */
    template<typename S, typename... Args>
    std::unique_ptr<Session> makeSession(Args&&... args) const
    {
        return std::make_unique<S>(name_, uuid_, token_,
            zctx_.get(), zfins_.get(), zopr_.get(), zevs_.get(),
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
    ///@{
    void sendOperation(Operation::Type oper) noexcept;
    void sendOperation(Operation::Type oper, zmq::Part&& payload) noexcept;
    ///@}

    ///< Type used for function to receive an event.
    using EventRecvFunc = std::function<Event()>;
    ///< Type used for function to match events type.
    using EventMatchFunc = std::function<bool(Event::Type)>;

    /**
     * \brief Waits for events from the asynchronous task.
     *
     * This method shall be called from the main thread.
     * This method is thread-safe.
     *
     * \param[in] timeout Waiting deadline.
     * \param[in] match Function to match events, otherwise first one will be returned.
     *
     * \return The (optionally) received event.
     *
     * \see recvEvent()
     */
    ///@{
    Event waitForEvent(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1),
        EventMatchFunc match = {}) const;
    Event waitForEvent(zmq::Pollable& timeout, EventMatchFunc match = {}) const;
    Event waitForEvent(zmq::Pollable* timeout, EventMatchFunc match = {}) const;
    ///@}

    /**
     * \brief Gets the file descriptor of the events socket.
     *
     * The returned file descriptor can be used to wait for
     * events with an existing event loop.
     *
     * When the file descriptor is readable,
     * \ref waitForEvent shall be used passing
     * a 0s timeout, until no more events are available
     * (that is a \ref Event::Notification::Timeout notification).
     *
     * This function shall not fail.
     *
     * \return The file descriptor to poll.
     *
     * \see waitForEvent(std::chrono::milliseconds)
     */
    int eventFD() const;


private:
    friend class TestRunner;

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
    Event recvEvent() const;


private:
    const std::string name_;                   ///< Name.
    const Uuid uuid_;                          ///< Identifier.
    const std::unique_ptr<zmq::Context> zctx_; ///< ZMQ context.
    const std::unique_ptr<zmq::Socket> zops_;  ///< Inter-thread sending socket.
    const std::unique_ptr<zmq::Socket> zopr_;  ///< Inter-thread receiving socket.
    const std::unique_ptr<zmq::Socket> zevs_;  ///< Inter-thread events notifications.
    const std::unique_ptr<zmq::Socket> zevr_;  ///< Inter-thread events reception.
    const std::unique_ptr<zmq::Socket> zfins_; ///< Inter-thread send completion message.
    const std::unique_ptr<zmq::Socket> zfinr_; ///< Inter-thread recv completion message.

    /**
     * FIXME: to be removed when ZMQ_FD option can be accessed
     * for thread safe sockets:
     * https://github.com/zeromq/libzmq/issues/2941
     */
    const std::unique_ptr<zmq::Poller<zmq::Socket>> zevpoll_;

    mutable std::atomic<bool> running_; ///< Whether the task is running.
    SessionEnv::token_t token_;         ///< Current execution token for the task.

    std::vector<std::string> endpDelivery_; ///< List of endpoints.
    std::vector<std::string> endpDispatch_; ///< List of endpoints.
    std::vector<std::string> endpSnapshot_; ///< List of endpoints.
};

} // namespace fuurin

#endif // FUURIN_RUNNER_H
