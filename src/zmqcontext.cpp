/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/zmqcontext.h"
#include "fuurin/errors.h"
#include "failure.h"
#include "log.h"

#include <zmq.h>

#include <boost/scope_exit.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>


namespace fuurin {
namespace zmq {


/**
 * \brief ASIO context which runs asynchrous I/O, e.g. ASIO timers.
 */
struct Context::IOWork
{
    boost::asio::io_context io_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_ = boost::asio::make_work_guard(io_);

    void run() noexcept
    {
        try {
            io_.run();
        } catch (...) {
            LOG_FATAL(log::Arg{"Context::IOWork::run"sv},
                log::Arg{"unexpected exception caught!"sv});
        }
    }

    void stop() noexcept
    {
        try {
            io_.stop();
        } catch (...) {
            LOG_FATAL(log::Arg{"Context::IOWork::stop"sv},
                log::Arg{"unexpected exception caught!"sv});
        }
    }
};


Context::Context()
    : ptr_(zmq_ctx_new())
    , iowork_(std::make_unique<IOWork>())
{
    if (ptr_ == nullptr) {
        throw ERROR(ZMQContextCreateFailed, "could not create context",
            log::Arg{log::ec_t{zmq_errno()}});
    }

    bool commit = false;
    BOOST_SCOPE_EXIT(this, &commit)
    {
        if (!commit)
            terminate();
    };

    iocompl_ = std::async(std::launch::async, &IOWork::run, iowork_.get());
    commit = true;
}


Context::~Context() noexcept
{
    iowork_->stop();
    terminate();
}


void Context::terminate() noexcept
{
    int rc;

    do {
        rc = zmq_ctx_term(ptr_);
    } while (rc == -1 && zmq_errno() == EINTR);

    ASSERT(rc == 0, "zmq_ctx_term failed");
}


void* Context::zmqPointer() const noexcept
{
    return ptr_;
}


boost::asio::io_context& Context::ioContext() noexcept
{
    return iowork_->io_;
}

} // namespace zmq
} // namespace fuurin
