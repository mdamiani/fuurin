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


namespace fuurin {
namespace zmq {


/**
 * \brief This class wraps a ZMQ context.
 * This class is thread-safe.
 */
class Context final
{
    friend class Socket;

public:
    /**
     * \brief Initializes a ZMQ context.
     * This method calls \c zmq_ctx_new.
     * \exception ZMQContextCreateFailed Context could not be created.
     */
    Context();

    /**
     * \brief Stops this ZMQ context.
     * This method calls \c zmq_ctx_term.
     */
    ~Context() noexcept;

    /**
     * Disable copy.
     */
    ///{@
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    ///@}


private:
    void* const ptr_; ///< ZMQ context.
};
}
}

#endif // ZMQCONTEXT_H
