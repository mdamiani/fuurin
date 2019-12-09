/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef LOG_H
#define LOG_H

#include "fuurin/logger.h"

#include <boost/preprocessor/seq/for_each.hpp>

using namespace std::literals;


namespace fuurin {
namespace log {


/**
 * \brief Simple formatting function for logs.
 *
 * \param[in] format String format, in printf-style.
 * \param[in] ... Variable number of arguments.
 *
 * \return A formatted string.
 */
std::string format(const char* format, ...);


#define LOG_LEVEL(r, data, level) \
    template<typename... Args> \
    void level(Loc&& c, Args&&... args) \
    { \
        Arg arr[] = {args...}; \
        Logger::level(std::forward<Loc>(c), arr, sizeof...(Args)); \
    }
BOOST_PP_SEQ_FOR_EACH(LOG_LEVEL, _, (debug)(info)(warn)(error)(fatal))
#undef LOG_LEVEL


} // namespace log
} // namespace fuurin


#ifndef NDEBUG
#define LOG_DEBUG(...) fuurin::log::debug({__FILE__, __LINE__}, __VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#define LOG_INFO(...) fuurin::log::info({__FILE__, __LINE__}, __VA_ARGS__)
#define LOG_WARN(...) fuurin::log::warn({__FILE__, __LINE__}, __VA_ARGS__)
#define LOG_ERROR(...) fuurin::log::error({__FILE__, __LINE__}, __VA_ARGS__)
#define LOG_FATAL(...) fuurin::log::fatal({__FILE__, __LINE__}, __VA_ARGS__)

#endif // LOG_H
