#include "fuurin/errors.h"

#include <zmq.h>


namespace fuurin {
namespace err {


Error::Error(const log::Loc& loc, const char* what, const char* reason,
    const log::Arg& arg) noexcept
    : loc_(loc)
    , what_(what)
    , reason_(reason)
    , sys_(false)
    , ec_(0)
    , arg_(arg)
{
}


Error::Error(const log::Loc& loc, const char* what, int ec,
    const log::Arg& arg) noexcept
    : loc_(loc)
    , what_(what)
    , reason_(nullptr)
    , sys_(true)
    , ec_(ec)
    , arg_(arg)
{
}


const char* Error::what() const noexcept
{
    return what_;
}


const char* Error::reason() const noexcept
{
    if (sys_)
        return zmq_strerror(ec_);
    else
        return reason_;
}


bool Error::hasCode() const noexcept
{
    return sys_;
}


int Error::code() const noexcept
{
    return ec_;
}


const log::Loc& Error::loc() const noexcept
{
    return loc_;
}


const log::Arg& Error::arg() const noexcept
{
    return arg_;
}
}
}
