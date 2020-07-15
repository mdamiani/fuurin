/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/errors.h"

#include <zmq.h>


namespace fuurin {
namespace err {

Error::Error() noexcept
    : empty_(true)
    , what_(nullptr)
    , loc_({nullptr, 0})
{
}


Error::Error(const log::Loc& loc, const char* what, const log::Arg& arg) noexcept
    : empty_(false)
    , what_(what)
    , loc_(loc)
    , arg_(arg)
{
}


Error::~Error() noexcept
{
}


bool Error::empty() const noexcept
{
    return empty_;
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


std::ostream& operator<<(std::ostream& os, const Error& e)
{
    if (!e.empty())
        os << e.what() << e.arg();

    return os;
}

} // namespace err
} // namespace fuurin
