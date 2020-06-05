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

#include <functional>
#include <chrono>
#include <memory>


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
 *   - \ref State::Halted:
 *      The sockets are disconnected and the state machine is halted.
 *
 *   - \ref State::Trying:
 *      Announcements are sent through the \ref pong() function, every
 *      \ref timerTry_ interval. A transition forward to \ref Stable is
 *      performed if \ref ping() is called and \ref timerTry_ is
 *      stopped.
 *
 *   - \ref State::Stable:
 *      Replies are sent through \ref pong() function, upon request
 *      only, using \ref ping() method and \ref timerTmo_ is restarted.
 *      A transition back to \ref Trying is performed if \ref timerTmo_
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


public:
    /**
     * \brief Initializes the connection.
     * The connection state machine is initialized
     * and the initial state is \ref State::Halted.
     * Sockets are all closed.
     *
     * param[in] zctx ZMQ context.
     * param[in] doClose Function to call to close sockets.
     * param[in] doOpen Function to call to open sockets.
     * param[in] doPong Function to send reply to a ping.
     * param[in] onChange Function called whenever the state changes.
     */
    ConnMachine(zmq::Context* zctx,
        std::chrono::milliseconds retry,
        std::chrono::milliseconds timeout,
        std::function<void()> doClose,
        std::function<void()> doOpen,
        std::function<void()> doPong,
        std::function<void(State)> onChange);

    /**
     * \brief Destructor.
     */
    virtual ~ConnMachine() noexcept;

    /**
     * Disable copy.
     */
    ///{@
    ConnMachine(const ConnMachine&) = delete;
    ConnMachine& operator=(const ConnMachine&) = delete;
    ///@}

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
     * This timer is alwasy active, except in \ref State::Halted.
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
     * Causes \ref pong() to be called.
     *
     * \see state()
     */
    void onPing();

    /**
     * \brief Notifies \ref timerRetry() has fired.
     *
     * When in State::Trying, it causes:
     *  - if the timer has expired, then it calls Timer::consume().
     *  - \ref pong() is called.
     *
     * \exception Error if called when in State::Stable.
     */
    void onTimerRetryFired();

    /**
     * \brief Notifies \ref timerTimeout() has fired.
     *
     * When in any state, it causes:
     *  - if the timer has expired, then it calls Timer::consume().
     *  - \ref change(State) is called with \ref State::Trying.
     *  - \ref reconnect() is called.
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
    const std::function<void()> doClose_;       ///< Function to close sockets.
    const std::function<void()> doOpen_;        ///< Function to open sockets.
    const std::function<void()> doPong_;        ///< Function to send a reply to a ping.
    const std::function<void(State)> onChange_; ///< Function called whenever state changes.

    const std::unique_ptr<zmq::Timer> timerTry_; ///< Timer for announcements.
    const std::unique_ptr<zmq::Timer> timerTmo_; ///< Timer for connection timeout.

    State state_; ///< Connection state.
};

} // namespace fuurin

#endif // CONNMACHINE_H
