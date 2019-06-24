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


struct zmq_poller_event_t;


namespace fuurin {
namespace zmq {

class Socket;
template<typename...>
class Poller;
class PollerEvents;
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
     * \brief Initializes poller and add sockets.
     *
     * \param[in] array Array of sockets.
     * \param[in] size Size of the array.
     * \param[in] read Whether to wait for a read event.
     * \param[in] write Whether to wait for a write event.
     *
     * \exception ZMQPollerCreateFailed The poller could not be created.
     * \return A new ZMQ poller.
     *
     * \see destroyPoller(void**)
     */
    static void* createPoller(Socket* array[], size_t size, bool read, bool write);

    /**
     * \brief Destroys the poller and removes sockets.
     *
     * \param[in] A valid ZMQ poller.
     *
     * \see createPoller(Socket*[], size_t, bool, bool)
     */
    static void destroyPoller(void** ptr) noexcept;

    /**
     * \brief Waits for events from sockets.
     *
     * It calls \c zmq_poller_wait_all.
     *
     * \param[in] ptr ZMQ poller.
     * \param[out] events Array to store events.
     * \param[in] size Size of the array of events.
     * \param[in] timeout Timeout to wait for (-1 will disable timeout).
     *
     * \exception ZMQPollerWaitFailed Polling could not be performed.
     * \return Number of returned \c events, or 0 in case it timed out.
     */
    static size_t waitForEvents(void* ptr, zmq_poller_event_t events[], size_t size, int timeout = -1);
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
 * \brief This class takes one or more \ref Socket objects and performs polling.
 *
 * Sockets must be valid and open.
 * Before closing a socket, it must be removed first from the poller,
 * hence the poller must be destroyed.
 */
template<typename... Args>
class Poller final
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
     *
     * \see PollerImpl::createPoller(Socket*[], size_t, bool, bool)
     * \see sockets()
     */
    Poller(PollerEvents::Type ev, Args*... args)
        : sockets_({args...})
        , ptr_(internal::PollerImpl::createPoller(&sockets_[0], Count,
              ev == PollerEvents::Type::Read, ev == PollerEvents::Type::Write))
        , type_(ev)
        , timeout_(-1)
    {
    }

    /**
     * \brief Destroys the poller and removes all sockets.
     *
     * \see PollerImpl::destroyPoller(void**)
     */
    ~Poller() noexcept
    {
        internal::PollerImpl::destroyPoller(&ptr_);
    }

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
     * \param[in] ms Timeout in milliseconds,
     *      any negative value will disable timeout.
     *
     * \see timeout()
     * \see wait()
     */
    inline void setTimeout(int ms) noexcept
    {
        timeout_ = ms >= 0 ? ms : -1;
    }

    /**
     * \return The current timeout in milliseconds,
     *      or -1 in case of no timeout.
     */
    inline int timeout() const noexcept
    {
        return timeout_;
    }

    /**
     * \brief Waits for an event on multiple \ref Socket objects.
     * This function will block until at least one socket is ready,
     * or it will return after the configured \ref timeout().
     *
     * Returned \ref PollerEvents and \ref PollerIterator structures
     * are valid until this object is alive and until the next call
     * to this method.
     *
     * \exception ZMQPollerWaitFailed Polling could not be performed.
     * \return An iterable \ref PollerEvents object over the subset of ready sockets.
     *      This subset can be empty if \ref timeout() has expired and sockets are not ready.
     *
     * \see setTimeout(int)
     * \see PollerImpl::waitForEvents
     */
    inline PollerEvents wait()
    {
        return PollerEvents(type_, (zmq_poller_event_t*)raw_.events_,
            internal::PollerImpl::waitForEvents(ptr_,
                (zmq_poller_event_t*)raw_.events_, Count, timeout_));
    }


private:
    Array sockets_;           ///< Array of monitored sockets.
    void* ptr_;               ///< Pointer to ZMQ poller object.
    PollerEvents::Type type_; ///< Type of events to wait for.
    int timeout_;             ///< Timeout to wait for.

    /**
     * \brief This is the backing array to hold a bare \c zmq_poller_event_t array type.
     * Size and alignment depends on values found in zmq.h header file.
     */
    struct alignas(void*) Raw
    {
        char events_[32 * Count];
    } raw_;
};


} // namespace zmq
} // namespace fuurin

#endif // ZMQPOLLER_H
