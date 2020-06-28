/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/logger.h"
#include "failure.h"

#include <boost/preprocessor/seq/for_each.hpp>

#include <iostream>
#include <mutex>


namespace fuurin {
namespace log {


/**
 * ostream
 */

namespace {
inline void printkey(std::ostream& os, std::string_view key)
{
    if (!key.empty())
        os << key << ": ";
}

template<typename T>
inline void printarg(std::ostream& os, std::string_view key, T val)
{
    printkey(os, key);
    os << val;
}


inline void printargs(std::ostream& os, const Arg args[], size_t num)
{
    const char* const separator = ", ";
    const char* sep = "";

    for (size_t i = 0; i < num; ++i) {
        os << sep << args[i];
        sep = separator;
    }
}


std::mutex print_mutex;
inline void println(std::ostream& os, const Arg args[], size_t num)
{
    std::lock_guard<std::mutex> guard(print_mutex);
    printargs(os, args, num);
    os << "\n";
}
} // namespace


std::ostream& operator<<(std::ostream& os, const Arg& arg)
{
    switch (arg.type()) {
    case Arg::Type::Invalid:
        os << "<>";
        break;
    case Arg::Type::Int:
        printarg(os, arg.key(), arg.toInt());
        break;
    case Arg::Type::Double:
        printarg(os, arg.key(), arg.toDouble());
        break;
    case Arg::Type::Errno:
    case Arg::Type::String:
        printarg(os, arg.key(), arg.toString());
        break;
    case Arg::Type::Array:
        printkey(os, arg.key());
        printargs(os, arg.toArray(), arg.count());
        break;
    }

    return os;
}


void printArgs(std::ostream& os, const Arg args[], size_t num)
{
    println(os, args, num);
}


/**
 * Handler
 */

Handler::~Handler()
{
}


void StandardHandler::debug(const Loc&, const Arg args[], size_t num) const
{
    println(std::cout, args, num);
}


void StandardHandler::info(const Loc&, const Arg args[], size_t num) const
{
    println(std::cout, args, num);
}


void StandardHandler::warn(const Loc&, const Arg args[], size_t num) const
{
    println(std::cerr, args, num);
}


void StandardHandler::error(const Loc&, const Arg args[], size_t num) const
{
    println(std::cerr, args, num);
}


void StandardHandler::fatal(const Loc&, const Arg args[], size_t num) const
{
    println(std::cerr, args, num);
    std::abort();
}


void SilentHandler::debug(const Loc&, const Arg*, size_t) const
{
}


void SilentHandler::info(const Loc&, const Arg*, size_t) const
{
}


void SilentHandler::warn(const Loc&, const Arg*, size_t) const
{
}


void SilentHandler::error(const Loc&, const Arg*, size_t) const
{
}


void SilentHandler::fatal(const Loc&, const Arg args[], size_t num) const
{
    println(std::cerr, args, num);
    std::abort();
}


/**
 * Logger
 */

std::unique_ptr<Handler> Logger::handler_(new StandardHandler);

void Logger::installContentHandler(Handler* handler)
{
    ASSERT(handler != nullptr, "log message handler is null");

    handler_.reset(handler);
}


#define LOGGER_LEVEL(r, data, level) \
    void Logger::level(const Loc& loc, const Arg args[], size_t num) noexcept \
    { \
        try { \
            handler_->level(loc, args, num); \
        } catch (...) { \
            ASSERT(false, "Logger::" #level ", unexpected exception caught!"); \
        } \
    }
BOOST_PP_SEQ_FOR_EACH(LOGGER_LEVEL, _, (debug)(info)(warn)(error)(fatal))
#undef LOGGER_LEVEL

} // namespace log
} // namespace fuurin
