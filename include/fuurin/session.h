/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_SESSION_H
#define FUURIN_SESSION_H

#include "fuurin/sessionenv.h"
#include "fuurin/operation.h"
#include "fuurin/event.h"
#include "fuurin/uuid.h"

#include <memory>
#include <string>


namespace fuurin {

namespace zmq {
class Context;
class Socket;
class Part;
class Pollable;
class PollerWaiter;
} // namespace zmq


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
     * \param[in] name Session name.
     * \param[in] id Session identifier.
     * \param[in] token Session token, it's constant as long as this session is alive.
     * \param[in] zctx ZMQ context.
     * \param[in] zfin ZMQ socket to send completion event.
     * \param[in] zoper ZMQ socket to receive operation commands from main task.
     * \param[in] zevent ZMQ socket to send events to main task.
     */
    explicit Session(const std::string& name, Uuid id, SessionEnv::token_t token,
        zmq::Context* zctx, zmq::Socket* zfin, zmq::Socket* zoper,
        zmq::Socket* zevent);

    /**
     * \brief Destructor.
     */
    virtual ~Session() noexcept;

    /**
     * Disable copy.
     */
    ///@{
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
     * wait on the passed inter-thread \c sock and any other additional \ref zmq::Socket.
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
     * handle and define any other specific tasks than \ref Operation::Type::Stop.
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


protected:
    /**
     * \brief Receives an operation reply from a socket.
     *
     * \param[in] sock Socket the operation is received from.
     * \param[in] token Expected operation token.
     *
     * \return An operation object.
     */
    static Operation recvOperation(zmq::Socket* sock, SessionEnv::token_t token) noexcept;


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
    const std::string name_;          ///< Session Name.
    const Uuid uuid_;                 ///< Identifier.
    const SessionEnv::token_t token_; ///< Session token.
    zmq::Context* const zctx_;        ///< \see Runner::zctx_.
    zmq::Socket* const zfins_;        ///< \see Runner::zfins_.
    zmq::Socket* const zopr_;         ///< \see Runner::zopr_.
    zmq::Socket* const zevs_;         ///< \see Runner::zevs_.
};

} // namespace fuurin

#endif // FUURIN_SESSION_H
