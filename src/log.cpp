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
namespace log {

class DefaultHandler : public Handler
{
public:
    void debug(const Message& m)
    {
        std::cout << m.where << ": " << m.what << std::endl;
    }

    void info(const Message& m)
    {
        std::cout << m.where << ": " << m.what << std::endl;
    }

    void warn(const Message& m)
    {
        std::cerr << m.where << ": " << m.what << std::endl;
    }

    void error(const Message& m)
    {
        std::cerr << m.where << ": " << m.what << std::endl;
    }

    void fatal(const Message& m)
    {
        std::cerr << "FATAL at file " << m.file << " line " << m.line << ": " << m.where << ": "
                  << m.what << std::endl;
        std::abort();
    }
};

Handler::~Handler()
{}

std::unique_ptr<Handler> Logger::_handler(new DefaultHandler);

void Logger::installMessageHandler(Handler* handler)
{
    if (handler)
        _handler.reset(handler);
    else
        _handler.reset(new DefaultHandler);
}

void Logger::debug(const Message& message)
{
    _handler->debug(message);
}

void Logger::info(const Message& message)
{
    _handler->info(message);
}

void Logger::warn(const Message& message)
{
    _handler->warn(message);
}

void Logger::error(const Message& message)
{
    _handler->error(message);
}

void Logger::fatal(const Message& message)
{
    _handler->fatal(message);
}

std::string format(const char* format, ...)
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
}
