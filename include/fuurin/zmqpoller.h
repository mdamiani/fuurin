/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQPOLLER_H
#define ZMQPOLLER_H

#include <array>
#include <iterator>
#include <chrono>


struct zmq_poller_event_t;


namespace fuurin {
namespace zmq {

class Socket;
template<typename...>
class Poller;
class PollerObserver;
class PollerIterator;


namespace internal {

/**
 * Private implementation of the \ref Poller class.
 */
class PollerImpl final
{
    template<typename...>
    friend class fuurin::zmq::Poller;

private:
    /**
     * \brief Initializes poller and adds open sockets.
     *
     * The poller is added as observer to every socket.
     *
     * \param[in] obs The poller observer.
     * \param[in] array Array of sockets.
     * \param[in] size Size of the array.
     * \param[in] read Whether to wait for a read event.
     * \param[in] write Whether to wait for a write event.
     *
     * \exception ZMQPollerCreateFailed The poller could not be created.
     * \exception ZMQPollerAddSocketFailed Any open socket could not be added to the poller.
     * \return A new ZMQ poller.
     *
     * \see destroyPoller(void**, PollerObserver*, Socket**, size_t)
     */
    static void* createPoller(PollerObserver* obs, Socket* array[], size_t size, bool read, bool write);

    /**
     * \brief Adds a socket to a ZMQ poller.
     *
     * \param[in] poller A valid ZMQ poller.
     * \param[in] sock Socket to add.
     * \param[in] read Whether to wait for a read event.
     * \param[in] write Whether to wait for a write event.
     *
     * \exception ZMQPollerAddSocketFailed Socket is not open or it could not be added.
     */
    static void addSocket(void* poller, Socket* sock, bool read, bool write);

    /**
     * \brief Removes a socket from a ZMQ poller.
     *
     * \param[in] poller A valid ZMQ poller.
     * \param[in] sock Socket to remove.
     *
     * \exception ZMQPollerDelSocketFailed Socket is not open or it could not be removed.
     */
    static void delSocket(void* poller, Socket* sock);

    /**
     * \brief Adds or removes a socket from a ZMQ poller.
     *
     * The passed socket must be present in the list of socket being polled.
     * Socket is added if it's requested to either wait for a read or write event,
     * otherwise it is removed.
     *
     * \param[in] poller A valid ZMQ poller.
     * \param[in] array Socket being polled by the ZMQ poller.
     * \param[in] size Size of the array.
     * \param[in] read Whether to wait for a read event.
     * \param[in] write Whether to wait for a write event.
     */
    static void updateSocket(void* poller, Socket* array[], size_t size, Socket* sock, bool read, bool write);

    /**
     * \brief Destroys the poller and removes sockets.
     *
     * The poller is removed as observer from every socket.
     *
     * \param[in] poller A valid ZMQ poller.
     *
     * \see createPoller(PollerObserver*, Socket*[], size_t, bool, bool)
     */
    static void destroyPoller(void** poller, PollerObserver*, Socket* array[], size_t size) noexcept;

    /**
     * \brief Waits for events from sockets.
     *
     * It calls \c zmq_poller_wait_all.
     *
     * \param[in] ptr ZMQ poller.
     * \param[out] events Array to store events.
     * \param[in] size Size of the array of events.
     * \param[in] timeout Timeout to wait for (a negative value will disable timeout).
     *
     * \exception ZMQPollerWaitFailed Polling could not be performed.
     * \return Number of returned \c events, or 0 in case it timed out.
     */
    static size_t waitForEvents(void* ptr, zmq_poller_event_t events[], size_t size,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));
};

} // namespace internal


/**
 * \brief Events that are available after a \ref Poller::wait() action.
 */
class PollerEvents final
{
public:
    /**
     * \brief Type of event to poll.
     */
    enum Type
    {
        Read,  ///< Read event.
        Write, ///< Write event.
    };

    /**
     * \brief Initializes the list of events.
     *
     * \param[in] type Type of events.
     * \param[in] first First event of the array.
     * \param[in] size Number of events.
     */
    explicit PollerEvents(Type type, zmq_poller_event_t* first = nullptr, size_t size = 0) noexcept;

    /**
     * \return Type of events.
     */
    Type type() const noexcept;

    /**
     * \return Number of events.
     */
    size_t size() const noexcept;

    /**
     * \return Whether there are no events.
     */
    bool empty() const noexcept;

    /**
     * \brief Direct access to element.
     * No boundary checking is performed.
     *
     * \param[in] pos Position of the element.
     *
     * \return The element at the specified position \c pos.
     */
    Socket* operator[](size_t pos) const;

    /**
     * \return An iterator to the being of events.
     */
    PollerIterator begin() const noexcept;

    /**
     * \return An iterator to the end of events.
     */
    PollerIterator end() const noexcept;


private:
    const Type type_;                 ///< Type of events.
    zmq_poller_event_t* const first_; ///< First event of the array.
    const size_t size_;               ///< Number of events.
};


/**
 * \brief Iterator over the available events after a \ref Poller::wait() action.
 */
class PollerIterator final : public std::iterator<std::input_iterator_tag, Socket*>
{
public:
    /**
     * \brief Initializes this iterator.
     * \param[in] ev Pointer to the actual ZMQ poller event.
     */
    explicit PollerIterator(zmq_poller_event_t* ev = nullptr) noexcept;

    /**
     * \brief Post increment operator.
     * \retern Reference to this incremented iterator.
     */
    PollerIterator& operator++() noexcept;

    /**
     * \brief Pre increment operator.
     * \return The iterator before the actual increment.
     */
    PollerIterator operator++(int) noexcept;

    /**
     * \brief Comparison operator.
     * \param[in] other Another iterator.
     * \return \c true when the iterators are pointing to the same ZMQ poller event.
     */
    ///{@
    bool operator==(PollerIterator other) const noexcept;
    bool operator!=(PollerIterator other) const noexcept;
    ///@}

    /**
     * \brief Dereference operator.
     * \return A pointer to the \ref Socket object bound to this ZMQ poller event.
     */
    value_type operator*() const noexcept;


private:
    zmq_poller_event_t* ev_; ///< ZMQ poller event.
};


/**
 * \brief This interface wraps the waiting primitive upon one or more \ref Socket.
 *
 * This interface is implemented by the actual \ref Poller class.
 */
class PollerWaiter
{
public:
    /// Constructor.
    PollerWaiter();

    /// Destructor.
    virtual ~PollerWaiter() noexcept;

    /**
     * Disable copy.
     */
    ///{@
    PollerWaiter(const PollerWaiter&) = delete;
    PollerWaiter& operator=(const PollerWaiter&) = delete;
    ///@}

    /**
     * \brief Sets the timeout of a \ref wait() operation.
     *
     * \param[in] tmeo Timeout value, any negative value will disable timeout.
     *
     * \see timeout()
     * \see wait()
     */
    virtual void setTimeout(std::chrono::milliseconds tmeo) noexcept = 0;

    /**
     * \return The current timeout in milliseconds,
     *      or a negative value (-1ms) if timeout is disabled.
     *
     * \see setTimeout(std::chrono::milliseconds)
     * \see wait()
     */
    virtual std::chrono::milliseconds timeout() const noexcept = 0;

    /**
     * \brief Waits for an event on multiple \ref Socket objects.
     * This method will block forever until at least one socket is ready,
     * or it will return after \ref timeout() has expired.
     *
     * Returned \ref PollerEvents and \ref PollerIterator structures
     * are valid until this object is alive and until the next call
     * to this method.
     *
     * \exception ZMQPollerWaitFailed Polling could not be performed.
     * \return An iterable \ref PollerEvents object over the subset of ready sockets.
     *      This subset can be empty if \ref timeout() has expired and sockets are not ready.
     *
     * \see setTimeout(std::chrono::milliseconds)
     * \see PollerWaiter::wait()
     */
    virtual PollerEvents wait() = 0;
};


/**
 * \brief This interface wraps the observer behavior a poller upon a socket.
 *
 * Whenever the state of a socket has changed, the poller gets notified through
 * this interface.
 */
class PollerObserver
{
public:
    /// Constructor.
    PollerObserver();

    /// Destructor.
    virtual ~PollerObserver() noexcept;

    /**
     * Disable copy.
     */
    ///{@
    PollerObserver(const PollerObserver&) = delete;
    PollerObserver& operator=(const PollerObserver&) = delete;
    ///@}


public:
    // TODO: make observer inteface loose coupled and make remove these public methods.
    /**
     * \brief Updates the state of the specified \ref Socket.
     *
     * Observer gets notified by any socket when its state changes.
     * The state of a socket changes upong a call to \ref Socket::connect(),
     * \ref Socket::bind() or \ref Socket::close().
     *
     * This method is intended to be automatically called by a \ref Socket
     * whenever its connection status changes.
     *
     * \param[in] sock Socket whose state changed.
     *
     * \exception ZMQPollerAddSocketFailed When the passed \ref Socket could not be added to poller.
     * \exception ZMQPollerDelSocketFailed When the passed \ref Socket could not be removed from poller.
     */
    ///{@
    virtual void updateOnOpen(Socket* sock) = 0;
    virtual void updateOnClose(Socket* sock) = 0;
    ///@}
};


/**
 * \brief This class takes one or more \ref Socket objects and performs polling.
 *
 * This poller and every passed sockets must belong and operate on
 * the SAME execution thread, otherwise the behavior is undefined.
 *
 * Sockets must be valid. This poller is registered as observer of
 * every passed socket, which is automatically added to the poller
 * on open and removed on close.
 */
template<typename... Args>
class Poller final : public PollerWaiter, public PollerObserver
{
    friend class internal::PollerImpl;

public:
    constexpr static size_t Count = sizeof...(Args); ///< Number of sockets;
    using Array = std::array<Socket*, Count>;        ///< Poller array type.


public:
    /**
     * \brief Creates the poller and adds the specified sockets.
     *
     * \param[in] ev Type of event to poll.
     * \param[in] args Sockets to poll for events.
     *      Every socket must be valid and open.
     *
     * \exception ZMQPollerCreateFailed The poller could not be created.
     * \exception ZMQPollerAddSocketFailed Any open socket could not be added to the poller.
     *
     * \see PollerImpl::createPoller(PollerObserver*, Socket*[], size_t, bool, bool)
     * \see sockets()
     */
    Poller(PollerEvents::Type ev, Args*... args)
        : sockets_({args...})
        , ptr_(internal::PollerImpl::createPoller(this, &sockets_[0], Count,
              ev == PollerEvents::Type::Read, ev == PollerEvents::Type::Write))
        , type_(ev)
        , timeout_(std::chrono::milliseconds(-1))
    {
    }

    /**
     * \brief Destroys the poller and removes all sockets.
     *
     * \see PollerImpl::destroyPoller(void**, PollerObserver*, Socket**, size_t)
     */
    ~Poller() noexcept
    {
        internal::PollerImpl::destroyPoller(&ptr_, this, &sockets_[0], Count);
    }

    /**
     * Disable copy.
     */
    ///{@
    Poller(const Poller&) = delete;
    Poller& operator=(const Poller&) = delete;
    ///@}

    /**
     * \return Array of sockets of this poller.
     */
    inline const Array& sockets() const noexcept
    {
        return sockets_;
    }

    /**
     * \return The type of events to wait for.
     */
    inline PollerEvents::Type type() const noexcept
    {
        return type_;
    }

    /**
     * \brief Sets the timeout of a \ref wait() operation.
     *
     * \param[in] tmeo Timeout value, any negative value will disable timeout.
     *
     * \see PollerWaiter::setTimeout
     */
    inline void setTimeout(std::chrono::milliseconds tmeo) noexcept override
    {
        timeout_ = tmeo.count() >= 0 ? tmeo : std::chrono::milliseconds(-1);
    }

    /**
     * \return The current timeout in milliseconds,
     *      or a negative value (-1ms) if timeout is disabled.
     *
     * \see PollerWaiter::timeout()
     */
    inline std::chrono::milliseconds timeout() const noexcept override
    {
        return timeout_;
    }

    /**
     * \brief Adds the observed socket to this poller.
     *
     * \see PollerObserver::updateOnOpen
     */
    void updateOnOpen(Socket* sock) override
    {
        internal::PollerImpl::updateSocket(ptr_, &sockets_[0], Count, sock,
            type_ == PollerEvents::Type::Read, type_ == PollerEvents::Type::Write);
    }

    /**
     * \brief Removes the observed socket from this poller.
     *
     * \see PollerObserver::updateOnClose
     */
    void updateOnClose(Socket* sock) override
    {
        internal::PollerImpl::updateSocket(ptr_, &sockets_[0], Count, sock, false, false);
    }

    /**
     * \brief Waits for an event on multiple \ref Socket objects.
     *
     * \exception ZMQPollerWaitFailed Polling could not be performed.
     * \return An iterable \ref PollerEvents object over the subset of ready sockets.
     *
     * \see PollerWaiter::wait()
     * \see PollerImpl::waitForEvents
     */
    inline PollerEvents wait() override
    {
        return PollerEvents(type_, (zmq_poller_event_t*)raw_.events_,
            internal::PollerImpl::waitForEvents(ptr_,
                (zmq_poller_event_t*)raw_.events_, Count, timeout_));
    }


private:
    Array sockets_;                     ///< Array of monitored sockets.
    void* ptr_;                         ///< Pointer to ZMQ poller object.
    PollerEvents::Type type_;           ///< Type of events to wait for.
    std::chrono::milliseconds timeout_; ///< Timeout to wait for.

    /**
     * \brief This is the backing array to hold a bare \c zmq_poller_event_t array type.
     * Size and alignment depend on values found in zmq.h header file.
     */
    struct alignas(void*) Raw
    {
        char events_[32 * Count];
    } raw_;
};


} // namespace zmq
} // namespace fuurin

#endif // ZMQPOLLER_H
