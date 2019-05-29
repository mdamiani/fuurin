#include "fuurin/errors.h"

#include <zmq.h>


namespace fuurin {
namespace err {


Error::Error(const log::Loc& loc, const char* what, const log::Arg& arg) noexcept
    : what_(what)
    , loc_(loc)
    , arg_(arg)
{
}


const char* Error::what() const noexcept
{
    return what_;
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
