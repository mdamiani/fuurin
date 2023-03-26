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
#include <type_traits>


struct zmq_poller_event_t;


namespace fuurin {
namespace zmq {

template<typename...>
class Poller;
template<typename...>
class PollerAuto;
class PollerObserver;
class PollerIterator;
class Pollable;


namespace internal {

/**
 * Private implementation of the \ref Poller class.
 */
class PollerImpl final
{
    template<typename...>
    friend class fuurin::zmq::Poller;
    template<typename...>
    friend class fuurin::zmq::PollerAuto;

private:
    /**
     * \brief Initializes poller and adds open pollable sockets.
     *
     * The poller is added as observer to every socket, if \c obs is not nullptr.
     *
     * \param[in] obs The poller observer, or nullptr.
     * \param[in] array Array of pollable sockets.
     * \param[in] size Size of the array.
     * \param[in] read Whether to wait for a read event.
     * \param[in] write Whether to wait for a write event.
     *
     * \exception ZMQPollerCreateFailed The poller could not be created.
     * \exception ZMQPollerAddSocketFailed Any open socket could not be added to the poller.
     * \return A new ZMQ poller.
     *
     * \see destroyPoller(void**, PollerObserver*, Pollable**, size_t)
     */
    static void* createPoller(PollerObserver* obs, Pollable* array[], size_t size, bool read, bool write);

    /**
     * \brief Adds a pollable socket to a ZMQ poller.
     *
     * \param[in] poller A valid ZMQ poller.
     * \param[in] sock Socket to add.
     * \param[in] read Whether to wait for a read event.
     * \param[in] write Whether to wait for a write event.
     *
     * \exception ZMQPollerAddSocketFailed Socket is not open or it could not be added.
     */
    static void addSocket(void* poller, Pollable* sock, bool read, bool write);

    /**
     * \brief Removes a pollable socket from a ZMQ poller.
     *
     * \remark Raises a fatal error if \c sock is not open,
     *         or it could not be removed from ZMQ \c poller.
     *
     * \param[in] poller A valid ZMQ poller.
     * \param[in] sock Socket to remove.
     */
    static void delSocket(void* poller, Pollable* sock) noexcept;

    /**
     * \brief Adds or removes a pollable socket from a ZMQ poller.
     *
     * The passed socket must be present in the list of socket being polled.
     * Socket is added if it's requested to either wait for a read or write event,
     * otherwise it is removed.
     *
     * \param[in] poller A valid ZMQ poller.
     * \param[in] array Array of pollable sockets.
     * \param[in] size Size of the array.
     * \param[in] sock Socket being updated on the ZMQ \c poller.
     * \param[in] read Whether to wait for a read event.
     * \param[in] write Whether to wait for a write event.
     */
    static void updateSocket(void* poller, Pollable* array[], size_t size, Pollable* sock, bool read, bool write);

    /**
     * \brief Destroys the poller and removes sockets.
     *
     * The poller is removed as observer from every socket, if \c obs is not nullptr.
     *
     * \param[in] poller A valid ZMQ poller.
     * \param[in] obs The poller observer, or nullptr.
     * \param[in] array Array of sockets.
     * \param[in] size Size of the array.
     *
     * \see createPoller(PollerObserver*, Pollable*[], size_t, bool, bool)
     */
    static void destroyPoller(void** poller, PollerObserver* obs, Pollable* array[], size_t size) noexcept;

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
    Pollable* operator[](size_t pos) const;

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
class PollerIterator final
{
public:
    /**
     * Iterator traits.
     */
    ///@{
    using iterator_category = std::input_iterator_tag;
    using value_type = Pollable*;
    using difference_type = value_type;
    using pointer = value_type*;
    using reference = value_type&;
    ///@}


public:
    /**
     * \brief Initializes this iterator.
     * \param[in] ev Pointer to the actual ZMQ poller event.
     */
    explicit PollerIterator(zmq_poller_event_t* ev = nullptr) noexcept;

    /**
     * \brief Post increment operator.
     * \return Reference to this incremented iterator.
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
    ///@{
    bool operator==(PollerIterator other) const noexcept;
    bool operator!=(PollerIterator other) const noexcept;
    ///@}

    /**
     * \brief Dereference operator.
     * \return A pointer to the \ref Pollable object bound to this ZMQ poller event.
     */
    value_type operator*() const noexcept;


private:
    zmq_poller_event_t* ev_; ///< ZMQ poller event.
};


/**
 * \brief This interface wraps the waiting primitive upon one or more \ref Pollable sockets.
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
    ///@{
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
     * \brief Waits for an event on multiple \ref Pollable sockets.
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
 * \brief This interface wraps the observer behavior of a poller upon a socket.
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
    ///@{
    PollerObserver(const PollerObserver&) = delete;
    PollerObserver& operator=(const PollerObserver&) = delete;
    ///@}


public:
    // TODO: make observer inteface loose coupled and remove these public methods.
    /**
     * \brief Updates the state of the specified \ref Pollable socket.
     *
     * Observer gets notified by any socket when its state changes.
     * The state of a socket changes upon a call to \ref Socket::connect(),
     * \ref Socket::bind() or \ref Socket::close().
     *
     * This method is intended to be automatically called by a \ref Socket
     * whenever its connection status changes.
     *
     * This method must operate on an open socket.
     *
     * \param[in] sock Socket whose state changed, it must be open.
     *
     * \exception ZMQPollerAddSocketFailed When the passed \ref Socket could not be added to poller.
     *
     * \see Socket::isOpen()
     */
    ///@{
    virtual void updateOnOpen(Pollable* sock) = 0;
    virtual void updateOnClose(Pollable* sock) = 0;
    ///@}
};


/**
 * \brief This class takes one or more \ref Pollable sockets and performs polling.
 *
 * Sockets must be valid and already open.
 *
 * This poller and every passed sockets must belong and operate on
 * the SAME execution thread, otherwise the behavior is undefined,
 * unless sockets are thread-safe (e.g. Client/Server or Radio/Dish).
 */
template<typename... Args>
class Poller : public PollerWaiter
{
    friend class internal::PollerImpl;

public:
    constexpr static size_t Count = sizeof...(Args); ///< Number of sockets;
    using Array = std::array<Pollable*, Count>;      ///< Poller array type.


protected:
    /**
     * \brief Creates the poller and adds the specified sockets.
     *
     * When a \ref PollerObserver is registered as observer of every passed socket,
     * they are automatically added/removed to the polling list upon their open/close.
     *
     * If \ref PollerObserver is not specified, then every socket must be already open.
     *
     * \param[in] ev Type of event to poll.
     * \param[in] obs Whether to observe the passed socket for open/close events, otherwhise \c nullptr.
     * \param[in] args Sockets to poll for events.
     *
     * \exception ZMQPollerCreateFailed The poller could not be created.
     * \exception ZMQPollerAddSocketFailed Any open socket could not be added to the poller.
     *
     * \see PollerImpl::createPoller(PollerObserver*, Pollable*[], size_t, bool, bool)
     * \see sockets()
     */
    Poller(PollerEvents::Type ev, PollerObserver* obs, Args*... args)
        : sockets_({args...})
        , ptr_(internal::PollerImpl::createPoller(obs, &sockets_[0], Count,
              ev == PollerEvents::Type::Read, ev == PollerEvents::Type::Write))
        , type_(ev)
        , obs_(obs)
        , timeout_(std::chrono::milliseconds(-1))
    {
        constexpr bool check_type = (std::is_convertible_v<Args*, Pollable*> && ...);
        static_assert(check_type, "Poller arguments must be of type fuurin::zmq::Pollable*");
    }

    /**
     * \brief Creates a poller with timeout.
     *
     * \param[in] ev Type of event to poll.
     * \param[in] tmeo Timeout value.
     * \param[in] obs Observer interface.
     * \param[in] args Sockets to poll for events.
     *
     * \see Poller(PollerEvents::Type, PollerObserver*, Args*...)
     * \see setTimeout(std::chrono::milliseconds)
     */
    Poller(PollerEvents::Type ev, std::chrono::milliseconds tmeo, PollerObserver* obs, Args*... args)
        : Poller{ev, obs, args...}
    {
        setTimeout(tmeo);
    }


public:
    /**
     * \brief Creates the poller and adds the specified sockets.
     *
     * \param[in] ev Type of event to poll.
     * \param[in] args Sockets to poll for events, they must be already open.
     *      Every socket must be valid and open.
     *
     * \exception ZMQPollerCreateFailed The poller could not be created.
     * \exception ZMQPollerAddSocketFailed Any open socket could not be added to the poller.
     *
     * \see PollerImpl::createPoller(PollerObserver*, Pollable*[], size_t, bool, bool)
     * \see sockets()
     */
    Poller(PollerEvents::Type ev, Args*... args)
        : Poller(ev, nullptr, args...)
    {
    }

    /**
     * \brief Creates the poller with timeout.
     *
     * \param[in] ev Type of event to poll.
     * \param[in] tmeo Timeout value.
     * \param[in] args Sockets to poll for events.
     *
     * \see Poller(PollerEvents::Type, Args*...)
     */
    Poller(PollerEvents::Type ev, std::chrono::milliseconds tmeo, Args*... args)
        : Poller(ev, tmeo, nullptr, args...)
    {
    }

    /**
     * \brief Destroys the poller and removes all sockets.
     *
     * \see PollerImpl::destroyPoller(void**, PollerObserver*, Pollable**, size_t)
     */
    virtual ~Poller() noexcept
    {
        internal::PollerImpl::destroyPoller(&ptr_, obs_, &sockets_[0], Count);
    }

    /**
     * Disable copy.
     */
    ///@{
    Poller(const Poller&) = delete;
    Poller& operator=(const Poller&) = delete;
    ///@}

    /**
     * \return The underlying raw ZMQ pointer.
     */
    inline void* zmqPointer() const noexcept
    {
        return ptr_;
    }

    /**
     * \return Array of sockets of this poller.
     */
    inline const Array& sockets() const noexcept
    {
        return sockets_;
    }

    /**
     * \return Array of sockets of this poller.
     */
    inline Array& sockets() noexcept
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
     * \brief Waits for an event on multiple \ref fuurin::zmq::Pollable sockets.
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
    PollerObserver* obs_;               ///< PollerObserver object.
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


/**
 * \brief Poller that monitors updates of sockets' state.
 *
 * This poller and every passed sockets must belong and operate on
 * the SAME execution thread, otherwise the behavior is undefined.
 *
 * \see Poller
 * \see PollerObserver
 */
template<typename... Args>
class PollerAuto : public Poller<Args...>, public PollerObserver
{
public:
    /**
     * \brief Initializes the poller and registers itself as a \ref PollerObserver for sockets.
     *
     * \see Poller::Poller(PollerEvents::Type, PollerObserver*, Args*... args)
     */
    PollerAuto(PollerEvents::Type ev, Args*... args)
        : Poller<Args...>(ev, this, args...)
    {
    }

    /**
     * \brief Initializes the poller with timeout and registers itself as a \ref PollerObserver for sockets.
     *
     * \see Poller::Poller(PollerEvents::Type, std::chrono::milliseconds, PollerObserver*, Args*... args)
     * \see Poller::setTimeout(std::chrono::milliseconds)
     */
    PollerAuto(PollerEvents::Type ev, std::chrono::milliseconds tmeo, Args*... args)
        : Poller<Args...>(ev, tmeo, this, args...)
    {
    }

    /**
     * \brief Destroys the poller and unregisters itself as a \ref PollerObserver from sockets.
     */
    virtual ~PollerAuto()
    {
    }

    /**
     * Disable copy.
     */
    ///@{
    PollerAuto(const PollerAuto&) = delete;
    PollerAuto& operator=(const PollerAuto&) = delete;
    ///@}

    /**
     * \brief Adds the observed socket to this poller.
     *
     * \see PollerObserver::updateOnOpen
     * \see PollerImpl::updateSocket(void*, Pollable**, size_t, Pollable*, bool, bool)
     */
    void updateOnOpen(Pollable* sock) override
    {
        internal::PollerImpl::updateSocket(Poller<Args...>::zmqPointer(),
            &Poller<Args...>::sockets()[0], Poller<Args...>::Count,
            sock, Poller<Args...>::type() == PollerEvents::Type::Read,
            Poller<Args...>::type() == PollerEvents::Type::Write);
    }

    /**
     * \brief Removes the observed socket from this poller.
     *
     * Socket must be open, otherwise a fatal error is raised.
     *
     * \see PollerObserver::updateOnClose
     * \see PollerImpl::updateSocket(void*, Pollable**, size_t, Pollable*, bool, bool)
     */
    void updateOnClose(Pollable* sock) override
    {
        internal::PollerImpl::updateSocket(Poller<Args...>::zmqPointer(),
            &Poller<Args...>::sockets()[0], Poller<Args...>::Count,
            sock, false, false);
    }
};

} // namespace zmq
} // namespace fuurin

#endif // ZMQPOLLER_H
