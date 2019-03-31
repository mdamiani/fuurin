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

void StandardHandler::debug(const Content& c)
{
    std::cout << c.where << ": " << c.what << std::endl;
}

void StandardHandler::info(const Content& c)
{
    std::cout << c.where << ": " << c.what << std::endl;
}

void StandardHandler::warn(const Content& c)
{
    std::cerr << c.where << ": " << c.what << std::endl;
}

void StandardHandler::error(const Content& c)
{
    std::cerr << c.where << ": " << c.what << std::endl;
}

void StandardHandler::fatal(const Content& c)
{
    abortWithContent(c);
}

void SilentHandler::debug(const Content&)
{}

void SilentHandler::info(const Content&)
{}

void SilentHandler::warn(const Content&)
{}

void SilentHandler::error(const Content&)
{}

void SilentHandler::fatal(const Content& c)
{
    abortWithContent(c);
}

std::unique_ptr<Handler> Logger::handler_(new StandardHandler);

void Logger::installContentHandler(Handler* handler)
{
    ASSERT(handler != nullptr, "log message handler is null");

    handler_.reset(handler);
}

void Logger::debug(const Content& c)
{
    handler_->debug(c);
}

void Logger::info(const Content& c)
{
    handler_->info(c);
}

void Logger::warn(const Content& c)
{
    handler_->warn(c);
}

void Logger::error(const Content& c)
{
    handler_->error(c);
}

void Logger::fatal(const Content& c)
{
    handler_->fatal(c);
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
