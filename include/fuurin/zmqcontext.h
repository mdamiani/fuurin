/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQCONTEXT_H
#define ZMQCONTEXT_H

#include <memory>
#include <future>


namespace boost {
namespace asio {
class io_context;
}
} // namespace boost


namespace fuurin {
namespace zmq {


/**
 * \brief This class wraps a ZMQ context.
 * This class is thread-safe.
 */
class Context final
{
public:
    /**
     * \brief Initializes a ZMQ context.
     * This method calls \c zmq_ctx_new.
     * \exception ZMQContextCreateFailed Context could not be created.
     */
    explicit Context();

    /**
     * \brief Stops this ZMQ context.
     * \see terminate()
     */
    ~Context() noexcept;

    /**
     * Disable copy.
     */
    ///@{
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    ///@}

    /**
     * This method is thread-safe.
     *
     * \return The underlying raw ZMQ pointer.
     */
    void* zmqPointer() const noexcept;

    // TODO: ioContext shall not be public.
    /**
     * \return The ASIO context.
     */
    boost::asio::io_context& ioContext() noexcept;


private:
    /**
     * \brief Terminates the ZMQ context.
     * This method calls \c zmq_ctx_term.
     * It panics in case the ZMQ context
     * could not be terminated.
     */
    void terminate() noexcept;


private:
    void* const ptr_; ///< ZMQ context.

    struct IOWork;
    std::unique_ptr<IOWork> iowork_; ///< ASIO context for asynchronous I/O.
    std::future<void> iocompl_;      ///< Future to wait for ASIO context completion.
};
} // namespace zmq
} // namespace fuurin

#endif // ZMQCONTEXT_H
