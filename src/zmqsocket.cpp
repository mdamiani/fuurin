#include "zmqsocket.h"
#include "zmqcontext.h"
#include "failure.h"
#include "log.h"
#include "fuurin/errors.h"

#include <zmq.h>
#include <boost/scope_exit.hpp>

#include <type_traits>
#include <chrono>
#include <thread>


namespace fuurin {
namespace zmq {


Socket::Socket(Context* ctx, int type)
    : ctx_(ctx)
    , type_(type)
    , ptr_(nullptr)
    , linger_(0)
{
}


Socket::~Socket() noexcept
{
    close();
}


Context* Socket::context() const noexcept
{
    return ctx_;
}


int Socket::type() const noexcept
{
    return type_;
}


void Socket::setLinger(int value) noexcept
{
    linger_ = value;
}


int Socket::linger() const noexcept
{
    return linger_;
}


void Socket::setSubscriptions(const std::list<std::string>& filters)
{
    subscriptions_ = filters;
}


const std::list<std::string>& Socket::subscriptions() const
{
    return subscriptions_;
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
    if (ptr_ != nullptr) {
        throw ERROR(ZMQSocketCreateFailed, "could not open socket",
            log::Arg{"reason"sv, "already open"sv});
    }

    bool commit = false;

    // Create socket.
    ptr_ = zmq_socket(ctx_->ptr_, type_);
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
    setOption(ZMQ_LINGER, linger_);

    for (const auto& s : subscriptions_)
        setOption(ZMQ_SUBSCRIBE, s);

    // Connect or bind socket.
    for (const auto& endp : endpoints_) {
        action(endp);
        const auto& lastEndp = getOption<std::string>(ZMQ_LAST_ENDPOINT);
        openEndpoints_.push_back(lastEndp);
    }

    commit = true;
}


void Socket::close() noexcept
{
    if (ptr_ == nullptr)
        return;

    try {
        const int rc = zmq_close(ptr_);
        ASSERT(rc == 0, "zmq_close failed");

        openEndpoints_.clear();
        ptr_ = nullptr;
    } catch (...) {
        ASSERT(false, "zmq_close threw exception");
    }
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
} // namespace zmq
} // namespace fuurin
