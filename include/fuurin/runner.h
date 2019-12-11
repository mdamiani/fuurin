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


namespace fuurin {

namespace zmq {
class Context;
class Socket;
class Part;
class PollerWait;
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
     * \brief Starts the backgroud thread of the main task.
     *
     * \return A future to hold the result (or an exception) of the computation,
     *      or and empty future in case the asynchronous task is already running.
     *
     * \exception Throws exceptions in case task could not be started.
     *
     * \see stop()
     * \see isRunning()
     * \see run(token_type_t)
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
     * \see run(token_type_t)
     */
    bool stop() noexcept;

    /**
     * \return Whether the asynchronous task is running.
     *
     * This method is thread-safe.
     *
     * \see start()
     * \see stop()
     * \see run(token_type_t)
     */
    bool isRunning() const noexcept;


private:
    typedef uint8_t token_type_t; ///< Type of the execution token.


protected:
    typedef uint8_t oper_type_t; ///< Type of operation.

    /**
     * \brief Type of operation to issue to the asynchronous task.
     *
     * \see sendOperation(Operation)
     * \see recvOperation()
     * \see operationReady(oper_type_t, zmq::Part&)
     */
    enum Operation : oper_type_t
    {
        Invalid, ///< Invalid command.
        Stop,    ///< Request to stop the asynchronous task.

        OPER_COUNT, ///< Number of operations.
    };


protected:
    /**
     * \brief Creates the poller to be used in the main asynchronous task.
     *
     * Concrete classes shall override this virtual method to create a poller in order to
     * wait on the passed inter-thread \c sock and any other additional \ref Socket.
     *
     * This notification shall be executed in the asynchronous task thread.
     *
     * \param[in] sock Inter-thread socket used by the main asynchronous task.
     *      It shall be added to the list of sockets \ref zmq::PollerWait will wait for.
     *
     * \exception May throw exceptions.
     * \return A pointer to a newly created poller.
     */
    virtual std::unique_ptr<zmq::PollerWait> createPoller(zmq::Socket* sock);

    /**
     * \brief Notifies whenever any operation request was received by the asynchrnous task.
     *
     * This notification shall be executed in the asynchronous task thread.
     *
     * Concrete classes shall override this virtual method in order to
     * handle and define any other specific tasks than \ref Operation::Stop.
     */
    virtual void operationReady(oper_type_t oper, const zmq::Part& payload);

    /**
     * \brief Notifies whenever any socket being polled is ready to be read by the asynchronous task.
     *
     * This notification shall be executed in the asynchronous task thread.
     *
     * Concrete classes shall override this virtual method in order to
     * handle their specific sockets. In case no additional sockets are
     * added by \ref createPoller (other than the inter-thread one),
     * then this nofitication is never issued.
     */
    virtual void socketReady(zmq::Socket* sock);

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
    void sendOperation(oper_type_t oper, zmq::Part& payload) noexcept;
    ///@}


private:
    /**
     * \brief Receives an operation to perform from the main thread.
     *
     * This method shall be called from the asynchronous task thread.
     *
     * The operation is received from the inter-thread communication socket.
     * Receive of operation shall not fail, i.e. no exceptions must be thrown
     * by the inter-thread socket, otherwise a fatal error is raised.
     *
     * \see run(token_type_t)
     */
    std::tuple<token_type_t, oper_type_t, zmq::Part> recvOperation() noexcept;

    /**
     * \brief Main body of the asynchronous task.
     *
     * This method represents the asynchrous taks, so it will be running
     * in a different thread than the main one.
     *
     * \param[in] token Token representing the current execution task,
     *      in order to filter out potential previous operations
     *      being read from the inter-thread socket.
     *
     * \exception Throws exceptions in case of unexpected errors.
     *
     * \see createPoller(zmq::Socket*)
     * \see operationReady(oper_type_t, const zmq::Part&)
     * \see socketReady(zmq::Socket*)
     * \see recvOperation()
     */
    void run(token_type_t token);


private:
    std::unique_ptr<zmq::Context> zctx_; ///< ZMQ context.
    std::unique_ptr<zmq::Socket> zops_;  ///< Inter-thread sending socket.
    std::unique_ptr<zmq::Socket> zopr_;  ///< Inter-thread receiving socket.
    std::atomic<bool> running_;          ///< Whether the task is running.
    token_type_t token_;                 ///< Current execution token for the task.
};

} // namespace fuurin

#endif // FUURIN_RUNNER_H
