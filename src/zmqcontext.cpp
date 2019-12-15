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

#include <zmq.h>


namespace fuurin {
namespace zmq {


Context::Context()
    : ptr_(zmq_ctx_new())
{
    if (ptr_ == nullptr) {
        throw ERROR(ZMQContextCreateFailed, "could not create context", log::Arg{log::ec_t{zmq_errno()}});
    }
}


Context::~Context() noexcept
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


} // namespace zmq
} // namespace fuurin
