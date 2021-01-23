/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SYNCMACHINE_H
#define SYNCMACHINE_H

#include "fuurin/uuid.h"

#include <functional>
#include <chrono>
#include <memory>
#include <string_view>
#include <ostream>


namespace fuurin {

namespace zmq {
class Context;
class Timer;
} // namespace zmq


/**
 * \brief State machine to performs the state synchronization.
 *
 * This class manages the synchronization through the usage of
 * multiple sockets, referred using a generic index. Thus, first
 * socket is referred with index 0, second one with index 1 and
 * so on. Same index is used for subsequent requests, unless
 * the connection is impossible. In this case the next socket
 * is used, by rotating the index value.
 *
 * The state machine has three states:
 *
 *   - \ref State::Halted.
 *     The sockets are disconnected and the state machiene is halted.
 *
 *   - \ref State::Download.
 *     Replies are being received from a synchronization request.
 *
 *   - \ref State::Synced.
 *     Synchronization is completed.
 *
 *   - \ref State::Failed.
 *     Synchronization is failed.
 */
class SyncMachine
{
public:
    /**
     * \brief State of synchronization.
     */
    enum struct State
    {
        Halted,   ///< Initial state.
        Download, ///< Dowloading data.
        Failed,   ///< Synchronization failure.
        Synced,   ///< Synchronization complete.
    };

    /**
     * \brief Kind of reply.
     */
    enum struct ReplyType
    {
        Snapshot, ///< Reply of snapshot.
        Complete, ///< Reply of completion.
    };

    /**
     * \brief How reply is acknowledged.
     */
    enum struct ReplyResult
    {
        Unexpected, ///< Reply was not expected.
        Discarded,  ///< Reply is discarded.
        Accepted,   ///< Reply is accepted.
    };

    ///< Data type for sequence number used in requests.
    using seqn_t = uint8_t;

    ///< Function type to close sockets.
    using CloseFunc = std::function<void(int idx)>;
    ///< Function type to open sockets.
    using OpenFunc = std::function<void(int idx)>;
    ///< Function type to send a synchronization request.
    using SyncFunc = std::function<void(int idx, seqn_t seqn)>;
    ///< Function type for change of state.
    using ChangeFunc = std::function<void(State)>;


public:
    /**
     * \brief Constructor.
     *
     * \param[in] name Description, used for logs and timers.
     * \param[in] uuid Uuid, used for logs;
     * \param[in] zctx ZMQ context.
     * \param[in] maxIndex Maximum index of sockets (must NOT be negative).
     * \param[in] maxRetry Maximum retry count (must NOT be negative).
     * \param[in] timeout Timeout (in ms) during downloading.
     * \param[in] close \see CloseFunc.
     * \param[in] open \see OpenFunc.
     * \param[in] sync \see SyncFunc.
     * \param[in] change \see ChangeFunc.
     */
    explicit SyncMachine(std::string_view name, Uuid uuid, zmq::Context* zctx,
        int maxIndex, int maxRetry, std::chrono::milliseconds timeout,
        CloseFunc close, OpenFunc open, SyncFunc sync, ChangeFunc change);

    /**
     * \brief Destructor.
     */
    virtual ~SyncMachine() noexcept;

    /**
     * Disable copy.
     */
    ///@{
    SyncMachine(const SyncMachine&) = delete;
    SyncMachine& operator=(const SyncMachine&) = delete;
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
     * \brief Return the timer used to check for connection timeout.
     *
     * This timer is active when in \ref State::Download.
     *
     * \return A pointer to the internal timer.
     */
    zmq::Timer* timerTimeout() const noexcept;

    /**
     * \return Maximum count of retry attempts.
     */
    int maxRetry() const noexcept;

    /**
     * \return Maximum value of sockets' index.
     */
    int maxIndex() const noexcept;

    /**
     * \brief Sets the next socket index to be used for a request.
     *
     * The next index is used only when a new request is started,
     * it doesn't immediately ovverride \ref currentIndex().
     *
     * The passed index is applied to a modulo of \ref maxIndex() + 1,
     * in order to always obtain a value in the range [0, \c maxIndex].
     * If the passed value is negative, then 0 is taken instead.
     *
     * \param[in] index Index value.
     *
     * \see nextIndex()
     */
    void setNextIndex(int index) noexcept;

    /**
     * \return The next socket index to be used for a request.
     *
     * \see setNextIndex(int)
     */
    int nextIndex() const noexcept;

    /**
     * \return The socket index being used for the current request.
     */
    int currentIndex() const noexcept;

    /**
     * \return The current retry count of a synchronization request.
     */
    int retryCount() const noexcept;

    /**
     * \return The current sequence number of a synchronization request.
     */
    seqn_t sequenceNumber() const noexcept;

    /**
     * \brief Notifies that the syncing is being halted.
     *
     * When either in \ref State::Download, \ref State::Synced or \ref State::Failed,
     * it causes:
     *   - A transition to \ref State::Halted.
     *   - Resets the internal state, e.g. \ref currentIndex()
     *     is cleared to 0.
     */
    void onHalt();

    /**
     * \brief Notifies that the syncing is being requested.
     *
     * When either in \ref State::Halted, \ref State::Synced or \ref State::Failed,
     * it causes:
     *   - A transition to \ref State::Download.
     *   - Synchronization starts.
     *
     * The index \ref nextIndex() is used to perform synchronization,
     * and then it becomes the \ref currentIndex().
     *
     * \see nextIndex()
     * \see currentIndex()
     */
    void onSync();

    /**
     * \brief Notifies that a reply was received.
     *
     * When in \ref State::Download, it causes:
     *   - A reply is discarded if it comes from a socket's index
     *     which doesn't match \ref currentIndex().
     *   - A reply is discarded if its sequence number
     *     doesn't match \ref sequenceNumber().
     *   - Time \ref timerTimeout() is restarted when
     *     synchronization is not yet completed.
     *   - A transition to \ref State::Synced when
     *     synchronization is completed.
     *
     * \param[in] index Index of the socket from where the \c reply was received.
     * \param[in] seqn  Sequence number of the corresponding request.
     * \param[in] reply Kind of reply from socket.
     *
     * \return Whether the reply is accepted.
     *
     * \see currentIndex()
     */
    ReplyResult onReply(int index, seqn_t seqn, ReplyType reply);

    /**
     * \brief Notifies that \ref timerTimeout() has fired.
     *
     * When in \ref State::Download, it causes:
     *   - If the timer has expired, then it calls \ref zmq::Timer::consume().
     *   - A transition to \ref State::Halted happens,
     *     if \ref maxRetry() exceeded.
     *   - the \ref currentIndex() is moved to
     *     \ref nextIndex()
     *   - Synchronization is restarted.
     */
    void onTimerTimeoutFired();


private:
    /**
     * \brief Halts the state machine.
     *
     * \param[in] indexClose Index to close.
     *
     * \see close(int)
     */
    void halt(int indexClose);

    /**
     * \brief Fails the state machine.
     *
     * \param[in] indexClose Index to close.
     *
     * \see close(int)
     */
    void fail(int indexClose);

    /**
     * \brief Sends a synchronization request.
     *
     * \param[in] indexClose Index to close.
     * \param[in] indexOpen Index to open.
     *
     * \see close(int)
     * \see open(int)
     * \see doSync_
     */
    void sync(int indexClose, int indexOpen);

    /**
     * \brief Closes a socket index.
     *
     * \param[in] index If not -1, then it is closed.
     *
     * \see doClose_
     */
    void close(int index);

    /**
     * \brief Opens a socket index.
     *
     * \param[in] index If not -1, then it is opened.
     *
     * \see doOpen_
     */
    void open(int index);

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
    const Uuid uuid_;             ///< Uuid to identify this state machine.
    const int indexMax_;          ///< Maximum sockets' index value.
    const int retryMax_;          ///< Maximum number of retry attempts.

    const CloseFunc doClose_;   ///< Function to close sockets.
    const OpenFunc doOpen_;     ///< Function to open sockets.
    const SyncFunc doSync_;     ///< Function to send a sync request.
    const ChangeFunc onChange_; ///< Function called whenever state changes.

    const std::unique_ptr<zmq::Timer> timerTmo_; ///< Timer for timeouts.

    State state_;   ///< Synchronization state.
    int indexCurr_; ///< Index of next request.
    int indexNext_; ///< Index of current request.
    int retryCurr_; ///< Retry number.
    seqn_t seqNum_; ///< Sequence number of request.
};


///< Streams to printable form a \ref SyncMachine::State value.
std::ostream& operator<<(std::ostream& os, const SyncMachine::State& en);

///< Streams to printable form a \ref SyncMachine::ReplyType value.
std::ostream& operator<<(std::ostream& os, const SyncMachine::ReplyType& en);

///< Streams to printable form a \ref SyncMachine::ReplyResult value.
std::ostream& operator<<(std::ostream& os, const SyncMachine::ReplyResult& en);

} // namespace fuurin

#endif // SYNCMACHINE_H
