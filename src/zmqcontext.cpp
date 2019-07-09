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
} // namespace zmq
} // namespace fuurin