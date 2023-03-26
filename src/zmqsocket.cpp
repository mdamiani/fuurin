/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/zmqsocket.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/errors.h"
#include "failure.h"
#include "types.h"
#include "log.h"

#include <zmq.h>
#include <boost/config.hpp> // for BOOST_LIKELY
#include <boost/scope_exit.hpp>

#include <type_traits>
#include <chrono>
#include <thread>
#include <algorithm>


namespace fuurin {
namespace zmq {

namespace {
int zmqSocketType(Socket::Type type)
{
    switch (type) {
    case Socket::Type::PAIR:
        return ZMQ_PAIR;
    case Socket::Type::PUB:
        return ZMQ_PUB;
    case Socket::Type::SUB:
        return ZMQ_SUB;
    case Socket::Type::REQ:
        return ZMQ_REQ;
    case Socket::Type::REP:
        return ZMQ_REP;
    case Socket::Type::DEALER:
        return ZMQ_DEALER;
    case Socket::Type::ROUTER:
        return ZMQ_ROUTER;
    case Socket::Type::PULL:
        return ZMQ_PULL;
    case Socket::Type::PUSH:
        return ZMQ_PUSH;
    case Socket::Type::SERVER:
        return ZMQ_SERVER;
    case Socket::Type::CLIENT:
        return ZMQ_CLIENT;
    case Socket::Type::RADIO:
        return ZMQ_RADIO;
    case Socket::Type::DISH:
        return ZMQ_DISH;
    }

    ASSERT(false, "invalid socket type");
    return -1;
}
} // namespace


Socket::Socket(Context* ctx, Type type) noexcept
    : ctx_(ctx)
    , type_(type)
    , ptr_(nullptr)
    , linger_(0ms)
    , hwmsnd_(0)
    , hwmrcv_(0)
    , conflate_(false)
{
}


Socket::~Socket() noexcept
{
    close();
}


void* Socket::zmqPointer() const noexcept
{
    return ptr_;
}


std::string Socket::description() const
{
    if (endpoints().empty())
        return std::string();

    return endpoints().front();
}


Context* Socket::context() const noexcept
{
    return ctx_;
}


Socket::Type Socket::type() const noexcept
{
    return type_;
}


void Socket::setLinger(std::chrono::milliseconds value) noexcept
{
    linger_ = value;
}


std::chrono::milliseconds Socket::linger() const noexcept
{
    return linger_;
}


void Socket::setHighWaterMark(int snd, int rcv) noexcept
{
    hwmsnd_ = snd;
    hwmrcv_ = rcv;
}


std::tuple<int, int> Socket::highWaterMark() const noexcept
{
    return std::make_tuple(hwmsnd_, hwmrcv_);
}


void Socket::setConflate(bool val) noexcept
{
    conflate_ = val;
}


bool Socket::conflate() const noexcept
{
    return conflate_;
}


int Socket::fileDescriptor() const
{
    if (!isOpen()) {
        throw ERROR(ZMQSocketOptionGetFailed, "could not get socket option",
            log::Arg{
                log::Arg{"reason"sv, "socket is closed"sv},
                log::Arg{"option"sv, ZMQ_FD},
            });
    }

    return getOption<int>(ZMQ_FD);
}


void Socket::setSubscriptions(const std::list<std::string>& filters)
{
    subscriptions_ = filters;
}


const std::list<std::string>& Socket::subscriptions() const
{
    return subscriptions_;
}


void Socket::setGroups(const std::list<std::string>& groups)
{
    groups_ = groups;
}


const std::list<std::string>& Socket::groups() const
{
    return groups_;
}


void Socket::setEndpoints(const std::list<std::string>& endpoints)
{
    endpoints_ = endpoints;
}


const std::list<std::string>& Socket::endpoints() const
{
    return endpoints_;
}


const std::list<std::string>& Socket::openEndpoints() const
{
    return openEndpoints_;
}


namespace {
template<typename T>
void static_assert_option_types()
{
    static_assert(std::is_same<T, int>::value, "socket option type not supported");
}
} // namespace


template<typename T>
void Socket::setOption(int option, const T& value)
{
    static_assert_option_types<T>();

    int rc;

    do {
        rc = zmq_setsockopt(ptr_, option, &value, sizeof(value));
    } while (rc == -1 && zmq_errno() == EINTR);

    if (rc == -1) {
        throw ERROR(ZMQSocketOptionSetFailed, "could not set socket option",
            log::Arg{
                log::Arg{"reason"sv, log::ec_t{zmq_errno()}},
                log::Arg{"option"sv, option},
            });
    }
}


template<>
void Socket::setOption(int option, const std::string& value)
{
    int rc;

    do {
        rc = zmq_setsockopt(ptr_, option, value.c_str(), value.size());
    } while (rc == -1 && zmq_errno() == EINTR);

    if (rc == -1) {
        throw ERROR(ZMQSocketOptionSetFailed, "could not set socket option",
            log::Arg{
                log::Arg{"reason"sv, log::ec_t{zmq_errno()}},
                log::Arg{"option"sv, option},
            });
    }
}


template<typename T>
T Socket::getOption(int option) const
{
    static_assert_option_types<T>();

    int rc;
    T optval;
    size_t optsize = sizeof(optval);

    do {
        rc = zmq_getsockopt(ptr_, option, &optval, &optsize);
    } while (rc == -1 && zmq_errno() == EINTR);

    if (rc == -1) {
        throw ERROR(ZMQSocketOptionGetFailed, "could not get socket option",
            log::Arg{
                log::Arg{"reason"sv, log::ec_t{zmq_errno()}},
                log::Arg{"option"sv, option},
            });
    }

    return optval;
}


template<>
std::string Socket::getOption(int option) const
{
    int rc;
    char optval[512];
    size_t optsize = sizeof(optval);

    do {
        rc = zmq_getsockopt(ptr_, option, optval, &optsize);
    } while (rc == -1 && zmq_errno() == EINTR);

    if (rc == -1) {
        throw ERROR(ZMQSocketOptionGetFailed, "could not get socket option",
            log::Arg{
                log::Arg{"reason"sv, log::ec_t{zmq_errno()}},
                log::Arg{"option"sv, option},
            });
    }

    return std::string(optval, optsize);
}


void Socket::connect()
{
    const auto action = static_cast<void (Socket::*)(const std::string&)>(&Socket::connect);
    open(std::bind(action, this, std::placeholders::_1));
}


void Socket::bind()
{
    const auto action = static_cast<void (Socket::*)(const std::string&, int)>(&Socket::bind);
    open(std::bind(action, this, std::placeholders::_1, 5000));
}


void Socket::open(std::function<void(std::string)> action)
{
    if (isOpen()) {
        throw ERROR(ZMQSocketCreateFailed, "could not open socket",
            log::Arg{"reason"sv, "already open"sv});
    }

    bool commit = false;

    // Create socket.
    ptr_ = zmq_socket(ctx_->zmqPointer(), zmqSocketType(type_));
    if (ptr_ == nullptr) {
        throw ERROR(ZMQSocketCreateFailed, "could not create socket",
            log::Arg{"reason"sv, log::ec_t{zmq_errno()}});
    }
    BOOST_SCOPE_EXIT(this, &commit)
    {
        if (!commit)
            close();
    };

    // Configure socket.
    setOption(ZMQ_LINGER, getMillis<int>(linger_));
    setOption(ZMQ_SNDHWM, hwmsnd_);
    setOption(ZMQ_RCVHWM, hwmrcv_);
    setOption(ZMQ_CONFLATE, conflate_ ? 1 : 0);

    for (const auto& s : subscriptions_)
        setOption(ZMQ_SUBSCRIBE, s);

    for (const auto& s : groups_)
        join(s);

    // Connect or bind socket.
    if (!endpoints_.empty()) {
        for (const auto& endp : endpoints_) {
            action(endp);
            const auto& lastEndp = getOption<std::string>(ZMQ_LAST_ENDPOINT);
            openEndpoints_.push_back(lastEndp);
        }
    } else {
        // raise an error due to empty endpoint.
        // TODO: any better method?
        action(std::string());
    }

    // Notify observers
    for (auto* poller : observers_)
        poller->updateOnOpen(this);

    commit = true;
}


void Socket::close() noexcept
{
    if (!isOpen())
        return;

    try {
        /**
         * In case open() raises and exception,
         * updateOnClose() is called on the overall
         * list of sockets, in order to restore state.
         * This implies that, for some sockets, the
         * updateOnOpen() might not even called.
         * For this reason, PollerImpl::delSocket(...)
         * has to manage the deletion of a socket from
         * poller which was not inserted before.
         */
        for (auto* poller : observers_)
            poller->updateOnClose(this);

        const int rc = zmq_close(ptr_);
        ASSERT(rc == 0, "zmq_close failed");

        openEndpoints_.clear();
        ptr_ = nullptr;
    } catch (...) {
        ASSERT(false, "socket close threw exception");
    }
}


bool Socket::isOpen() const noexcept
{
    return ptr_ != nullptr;
}


void Socket::connect(const std::string& endpoint)
{
    const int rc = zmq_connect(ptr_, endpoint.c_str());

    if (rc == -1) {
        throw ERROR(ZMQSocketConnectFailed, "could not connect socket",
            log::Arg{
                log::Arg{"reason"sv, log::ec_t{zmq_errno()}},
                log::Arg{"endpoint"sv, endpoint},
            });
    }
}


void Socket::bind(const std::string& endpoint, int timeout)
{
    namespace t = std::chrono;

    int rc;
    const auto start = t::steady_clock::now();

    do {
        rc = zmq_bind(ptr_, endpoint.c_str());

        if (rc != -1 || zmq_errno() != EADDRINUSE || timeout < 0)
            break;

        std::this_thread::sleep_for(t::milliseconds(10));

        const auto now = t::steady_clock::now();
        const auto msec = t::duration_cast<t::milliseconds>(now - start).count();

        if (timeout > 0 && msec >= timeout)
            break;
    } while (1);

    if (rc == -1) {
        throw ERROR(ZMQSocketBindFailed, "could not bind socket",
            log::Arg{
                log::Arg{"reason"sv, log::ec_t{zmq_errno()}},
                log::Arg{"endpoint"sv, endpoint},
                log::Arg{"timeout"sv, timeout},
            });
    }
}


namespace {
inline int sendMessagePart(void* socket, int flags, zmq_msg_t* msg)
{
    int rc;

    do {
        rc = zmq_msg_send(msg, socket, flags);
    } while (rc == -1 && zmq_errno() == EINTR);

    if (BOOST_UNLIKELY(rc == -1)) {
        // check for non-blocking mode
        if (zmq_errno() == EAGAIN)
            return -1;
        throw ERROR(ZMQSocketSendFailed, "could not send message part",
            log::Arg{"reason"sv, log::ec_t{zmq_errno()}});
    }

    return rc;
}


inline int recvMessagePart(void* socket, int flags, zmq_msg_t* msg)
{
    int rc;

    do {
        rc = zmq_msg_recv(msg, socket, flags);
    } while (rc == -1 && zmq_errno() == EINTR);

    if (BOOST_UNLIKELY(rc == -1)) {
        // check for non-blocking mode
        if (zmq_errno() == EAGAIN)
            return -1;
        throw ERROR(ZMQSocketRecvFailed, "could not recv message part",
            log::Arg{"reason"sv, log::ec_t{zmq_errno()}});
    }

    return rc;
}
} // namespace


int Socket::sendMessageMore(Part* part)
{
    return sendMessagePart(ptr_, ZMQ_SNDMORE, part->zmqPointer());
}


int Socket::sendMessageLast(Part* part)
{
    return sendMessagePart(ptr_, 0, part->zmqPointer());
}


int Socket::sendMessageTryMore(Part* part)
{
    return sendMessagePart(ptr_, ZMQ_DONTWAIT | ZMQ_SNDMORE, part->zmqPointer());
}


int Socket::sendMessageTryLast(Part* part)
{
    return sendMessagePart(ptr_, ZMQ_DONTWAIT, part->zmqPointer());
}


int Socket::recvMessageMore(Part* part)
{
    return recvMessagePart(ptr_, 0, part->zmqPointer());
}


int Socket::recvMessageLast(Part* part)
{
    return recvMessagePart(ptr_, 0, part->zmqPointer());
}


int Socket::recvMessageTryMore(Part* part)
{
    return recvMessagePart(ptr_, ZMQ_DONTWAIT, part->zmqPointer());
}


int Socket::recvMessageTryLast(Part* part)
{
    return recvMessagePart(ptr_, ZMQ_DONTWAIT, part->zmqPointer());
}


void Socket::join(const std::string& group)
{
    const int rc = zmq_join(ptr_, group.c_str());
    if (rc == -1) {
        throw ERROR(ZMQSocketGroupFailed, "could not join group",
            log::Arg{
                log::Arg{"reason"sv, log::ec_t{zmq_errno()}},
                log::Arg{"group"sv, group},
            });
    }
}

} // namespace zmq
} // namespace fuurin
