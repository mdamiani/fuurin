/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/zmqpoller.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/errors.h"
#include "failure.h"
#include "types.h"
#include "log.h"

#include <zmq.h>
#include <boost/scope_exit.hpp>

#include <algorithm>


namespace fuurin {
namespace zmq {
namespace internal {


void* PollerImpl::createPoller(PollerObserver* obs, Socket* array[], size_t size, bool read, bool write)
{
    // assert raw backing array
    static_assert(sizeof(Poller<Socket*>::Raw) >= sizeof(zmq_poller_event_t));
    static_assert(alignof(Poller<Socket*>::Raw) >= alignof(zmq_poller_event_t));

    // create poller
    void* ptr = zmq_poller_new();
    if (ptr == nullptr) {
        throw ERROR(ZMQPollerCreateFailed, "could not create poller",
            log::Arg{"reason"sv, log::ec_t{zmq_errno()}});
    }

    bool commit = false;
    BOOST_SCOPE_EXIT(&ptr, &commit, obs, array, size)
    {
        if (!commit)
            destroyPoller(&ptr, obs, array, size);
    };

    // add sockets
    for (size_t i = 0; i < size; ++i) {
        Socket* const s = array[i];

        s->registerPoller(obs);

        if (s->isOpen())
            addSocket(ptr, s, read, write);
    }

    commit = true;
    return ptr;
}


void PollerImpl::addSocket(void* ptr, Socket* s, bool read, bool write)
{
    ASSERT(s != nullptr, "poller was given a null socket");
    const short events = (ZMQ_POLLIN * !!read) | (ZMQ_POLLOUT * !!write);

    if (!s->isOpen()) {
        throw ERROR(ZMQPollerAddSocketFailed, "socket is not open",
            log::Arg{"endpoint"sv, !s->endpoints().empty() ? s->endpoints().front() : ""});
    }

    const int rc = zmq_poller_add(ptr, s->zmqPointer(), s, events);
    if (rc == -1) {
        throw ERROR(ZMQPollerAddSocketFailed, "could not add socket",
            log::Arg{"reason"sv, log::ec_t{zmq_errno()}});
    }
}


void PollerImpl::delSocket(void* ptr, Socket* s)
{
    ASSERT(s != nullptr, "poller was given a null socket");

    if (!s->isOpen()) {
        throw ERROR(ZMQPollerDelSocketFailed, "socket is not open",
            log::Arg{"endpoint"sv, !s->endpoints().empty() ? s->endpoints().front() : ""});
    }

    const int rc = zmq_poller_remove(ptr, s->zmqPointer());
    if (rc == -1) {
        throw ERROR(ZMQPollerDelSocketFailed, "could not remove socket",
            log::Arg{"reason"sv, log::ec_t{zmq_errno()}});
    }
}


void PollerImpl::updateSocket(void* ptr, Socket* array[], size_t size, Socket* s, bool read, bool write)
{
    const bool found = (std::find(array, array + size, s) != array + size);

    if (read || write) {
        if (!found) {
            throw ERROR(ZMQPollerAddSocketFailed, "could not add socket",
                log::Arg{"reason"sv, "socket not being polled"sv});
        }
        addSocket(ptr, s, read, write);
    } else {
        if (!found) {
            throw ERROR(ZMQPollerDelSocketFailed, "could not remove socket",
                log::Arg{"reason"sv, "socket not being polled"sv});
        }
        delSocket(ptr, s);
    }
}


void PollerImpl::destroyPoller(void** ptr, PollerObserver* obs, Socket* array[], size_t size) noexcept
{
    for (size_t i = 0; i < size; ++i) {
        Socket* const s = array[i];
        try {
            s->unregisterPoller(obs);
        } catch (...) {
            ASSERT(false, "failed to unregister poller observer");
        }
    }

    // destroy poller (thus remove any previously added sockets)
    const int rc = zmq_poller_destroy(ptr);
    ASSERT(rc != -1, "zmq_poller_destroy failed");
}


size_t PollerImpl::waitForEvents(void* ptr, zmq_poller_event_t events[], size_t size, std::chrono::milliseconds timeout)
{
    int rc;
    do {
        rc = zmq_poller_wait_all(ptr, events, size, getMillis<long>(timeout));
    } while (rc == -1 && zmq_errno() == EINTR);

    if (rc == -1) {
        // check whether timeout was expired.
        if (zmq_errno() == EAGAIN)
            return 0;

        throw ERROR(ZMQPollerWaitFailed, "could not wait for socket events",
            log::Arg{"reason"sv, log::ec_t{zmq_errno()}});
    }

    return size_t(rc);
}

} // namespace internal


PollerEvents::PollerEvents(Type type, zmq_poller_event_t* first, size_t size) noexcept
    : type_(type)
    , first_(first)
    , size_(size)
{
}


PollerEvents::Type PollerEvents::type() const noexcept
{
    return type_;
}


size_t PollerEvents::size() const noexcept
{
    return size_;
}


bool PollerEvents::empty() const noexcept
{
    return size_ == 0;
}


Socket* PollerEvents::operator[](size_t pos) const
{
    return *PollerIterator(first_ + pos);
}


PollerIterator PollerEvents::begin() const noexcept
{
    return PollerIterator(first_);
}


PollerIterator PollerEvents::end() const noexcept
{
    return PollerIterator(first_ + size_);
}


PollerIterator::PollerIterator(zmq_poller_event_t* ev) noexcept
    : ev_(ev)
{}


PollerIterator& PollerIterator::operator++() noexcept
{
    ++ev_;
    return *this;
}


PollerIterator PollerIterator::operator++(int) noexcept
{
    PollerIterator ret = *this;
    operator++();
    return ret;
}


bool PollerIterator::operator==(PollerIterator other) const noexcept
{
    return ev_ == other.ev_;
}


bool PollerIterator::operator!=(PollerIterator other) const noexcept
{
    return !(*this == other);
}


PollerIterator::value_type PollerIterator::operator*() const noexcept
{
    return static_cast<value_type>(ev_->user_data);
}


PollerWaiter::PollerWaiter() = default;
PollerWaiter::~PollerWaiter() noexcept = default;

PollerObserver::PollerObserver() = default;
PollerObserver::~PollerObserver() noexcept = default;

} // namespace zmq
} // namespace fuurin
