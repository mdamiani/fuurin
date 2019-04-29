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
#include "failure.h"

#include <boost/preprocessor/seq/for_each.hpp>

#include <cstdlib>
#include <cstdio>
#include <stdarg.h>
#include <iostream>
#include <vector>


namespace fuurin {
namespace log {

Handler::~Handler()
{}

namespace {
void abortWithContent(const Content& c)
{
    std::cerr << "FATAL at file " << c.file << " line " << c.line << ": " << c.where << ": "
              << c.what << std::endl;
    std::abort();
}
}

void StandardHandler::debug(const Content& c) const
{
    std::cout << c.where << ": " << c.what << std::endl;
}

void StandardHandler::info(const Content& c) const
{
    std::cout << c.where << ": " << c.what << std::endl;
}

void StandardHandler::warn(const Content& c) const
{
    std::cerr << c.where << ": " << c.what << std::endl;
}

void StandardHandler::error(const Content& c) const
{
    std::cerr << c.where << ": " << c.what << std::endl;
}

void StandardHandler::fatal(const Content& c) const
{
    abortWithContent(c);
}

void SilentHandler::debug(const Content&) const
{}

void SilentHandler::info(const Content&) const
{}

void SilentHandler::warn(const Content&) const
{}

void SilentHandler::error(const Content&) const
{}

void SilentHandler::fatal(const Content& c) const
{
    abortWithContent(c);
}


std::unique_ptr<Handler> Logger::handler_(new StandardHandler);

void Logger::installContentHandler(Handler* handler)
{
    ASSERT(handler != nullptr, "log message handler is null");

    handler_.reset(handler);
}

#define LOGGER_LEVEL(r, data, level) \
    void Logger::level(const Content& c) noexcept \
    { \
        try { \
            handler_->level(c); \
        } catch (...) { \
            abortWithContent( \
                {__LINE__, __FILE__, "Logger::" #level, "unexpected exception caught!"}); \
        } \
    }
BOOST_PP_SEQ_FOR_EACH(LOGGER_LEVEL, _, (debug)(info)(warn)(error)(fatal))
#undef LOGGER_LEVEL


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
}
}
