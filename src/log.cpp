/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "log.h"

#include <cstdio>
#include <stdarg.h>


namespace fuurin {
namespace log {


std::string format(const char* format, ...)
{
    va_list args;

    va_start(args, format);
    const int sz = std::vsnprintf(nullptr, 0, format, args);
    va_end(args);

    if (sz < 0) {
        // return a null string in case of errors
        return std::string();
    }

    std::string str(sz, 0);

    va_start(args, format);
    std::vsnprintf(&str[0], sz + 1, format, args);
    va_end(args);

    return str;
}
} // namespace log
} // namespace fuurin
