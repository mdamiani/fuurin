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

#include <cstdlib>
#include <cstdio>
#include <stdarg.h>
#include <iostream>
#include <vector>


namespace fuurin {


void logHandler(LogLevel level, const char *file, unsigned int line, const std::string &message)
{
    switch(level) {
    case DebugLevel:
    case InfoLevel:
    case WarningLevel:
    case ErrorLevel:
        std::cout << message << std::endl;
        break;

    case FatalLevel:
        std::cerr << "FATAL at file " << file << " line " << line << ": " << message << std::endl;
        std::abort();
        break;
    }
}


LogMessageHandler logMessage = logHandler;


std::string format(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    const int sz = std::vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);

    std::vector<char> buf(sz, 0);

    va_start(args, format);
    std::vsnprintf(&buf[0], sz, format, args);
    va_end(args);

    return std::string(buf.begin(), buf.end() - 1);
}


}
