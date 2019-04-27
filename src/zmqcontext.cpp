#include "zmqcontext.h"
#include "failure.h"
#include "fuurin/errors.h"

#include <zmq.h>


namespace fuurin {
namespace zmq {


Context::Context()
    : ptr_(zmq_ctx_new())
{
    if (ptr_ == nullptr) {
        throw ERROR(ZMQContextCreateFailed, "could not create context", zmq_errno());
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
}
}
