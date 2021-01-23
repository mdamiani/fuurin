/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CONNMACHINE_H
#define CONNMACHINE_H

#include "fuurin/uuid.h"

#include <functional>
#include <chrono>
#include <memory>
#include <string_view>


namespace fuurin {

namespace zmq {
class Context;
class Timer;
} // namespace zmq


/**
 * \brief State machine to establish the connection.
 *
 * The state machine has three states:
 *
 *   - \ref State::Halted.
 *      The sockets are disconnected and the state machine is halted.
 *
 *   - \ref State::Trying.
 *      Announcements are sent through the \ref doPong_ function, every
 *      \ref timerTry_ interval. A transition forward to \ref State::Stable is
 *      performed if \ref onPing() is called and \ref timerTry_ is
 *      stopped.
 *
 *   - \ref State::Stable.
 *      Replies are sent through \ref doPong_ function, upon request
 *      only, using \ref onPing() method and \ref timerTmo_ is restarted.
 *      A transition back to \ref State::Trying is performed if \ref timerTmo_
 *      expires.
 */
class ConnMachine
{
public:
    /**
     * \brief State of connection.
     */
    enum struct State
    {
        Halted, ///< Sockets are disconnected.
        Trying, ///< Sockets are connecting.
        Stable, ///< Sockets are connected.
    };

    ///< Function type to close sockets.
    using CloseFunc = std::function<void()>;
    ///< Function type to open sockets.
    using OpenFunc = std::function<void()>;
    ///< Function type to send a pong reply.
    using PongFunc = std::function<void()>;
    ///< Function type for change of state.
    using ChangeFunc = std::function<void(State)>;


public:
    /**
     * \brief Initializes the connection.
     *
     * The connection state machine is initialized
     * and the initial state is \ref State::Halted.
     * Sockets are all closed.
     *
     * \param[in] name Description, used for logs and timers.
     * \param[in] uuid Uuid, used for logs;
     * \param[in] zctx ZMQ context.
     * \param[in] retry Interval (in ms) for retry attempts.
     * \param[in] timeout Timeout (in ms) for the connection.
     * \param[in] doClose Function to call to close sockets.
     * \param[in] doOpen Function to call to open sockets.
     * \param[in] doPong Function to send reply to a ping.
     * \param[in] onChange Function called whenever the state changes.
     */
    explicit ConnMachine(std::string_view name, Uuid uuid, zmq::Context* zctx,
        std::chrono::milliseconds retry, std::chrono::milliseconds timeout,
        CloseFunc doClose, OpenFunc doOpen, PongFunc doPong, ChangeFunc onChange);

    /**
     * \brief Destructor.
     */
    virtual ~ConnMachine() noexcept;

    /**
     * Disable copy.
     */
    ///@{
    ConnMachine(const ConnMachine&) = delete;
    ConnMachine& operator=(const ConnMachine&) = delete;
    ///@}

    /**
     * \return Description for this state machine.
     */
    std::string_view name() const noexcept;

    /**
     * \return Uuid for this state machine.
     */
    Uuid uuid() const noexcept;

    /**
     * \return The current state.
     */
    State state() const noexcept;

    /**
     * \brief Returns the timer used to retry sending announcements.
     *
     * This timer is active when in \ref State::Trying.
     *
     * \return A pointer to the internal timer.
     */
    zmq::Timer* timerRetry() const noexcept;

    /**
     * \brief Return the timer used to check for connection timeout.
     *
     * This timer is always active, except in \ref State::Halted.
     * A transition to \ref State::Trying happens in case this timer expires.
     *
     * \return A pointer to the internal timer.
     */
    zmq::Timer* timerTimeout() const noexcept;

    /**
     * \brief Notifies that the connection shall be started.
     */
    void onStart();

    /**
     * \brief Notifies that the connection shall be stopped.
     */
    void onStop();

    /**
     * \brief Notifies a ping event due to a request from remote party.
     *
     * A transition to \ref State::Stable happens.
     * Timer \ref timerTry_ is stopped.
     * Causes \ref doPong_ to be called.
     *
     * \see state()
     */
    void onPing();

    /**
     * \brief Notifies \ref timerRetry() has fired.
     *
     * When in \ref State::Trying, it causes:
     *  - if the timer has expired, then it calls \ref zmq::Timer::consume().
     *  - \ref doPong_ is called.
     */
    void onTimerRetryFired();

    /**
     * \brief Notifies \ref timerTimeout() has fired.
     *
     * When in any state, it causes:
     *  - if the timer has expired, then it calls \ref zmq::Timer::consume().
     *  - \ref change(State) is called with \ref State::Trying.
     *  - \ref doClose_ and \ref doOpen_ are called.
     *  - \ref doPong_ is called.
     */
    void onTimerTimeoutFired();


private:
    /**
     * \brief (Re)Triggers the state machine.
     *
     * \see doOpen_
     * \see doClose_
     * \see doPong_
     */
    void trigger();

    /**
     * \brief Halts the state machine.
     *
     * \see doClose_
     */
    void halt();

    /**
     * \brief Change state to the new passed one.
     *
     * \param[in] state New state.
     *
     * \see onChange_
     */
    void change(State state);


private:
    const std::string_view name_; ///< Name to identify this state machine.
    const Uuid uuid_;             ///< Uuid of the state machine's compenent.

    const CloseFunc doClose_;   ///< Function to close sockets.
    const OpenFunc doOpen_;     ///< Function to open sockets.
    const PongFunc doPong_;     ///< Function to send a reply to a ping.
    const ChangeFunc onChange_; ///< Function called whenever state changes.

    const std::unique_ptr<zmq::Timer> timerTry_; ///< Timer for announcements.
    const std::unique_ptr<zmq::Timer> timerTmo_; ///< Timer for connection timeout.

    State state_; ///< Connection state.
};


///< Streams to printable form a \ref ConnMachine::State value.
std::ostream& operator<<(std::ostream& os, const ConnMachine::State& en);

} // namespace fuurin

#endif // CONNMACHINE_H
