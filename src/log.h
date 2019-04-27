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
#undef LOG

#ifndef NDEBUG
#define LOG_DEBUG(wh, wa) debug({__FILE__, __LINE__}, log::Arg(wh, wa))
#else
#define LOG_DEBUG(wh, wa)
#endif

#define LOG_INFO(wh, wa) info({__FILE__, __LINE__}, log::Arg(wh, wa))
#define LOG_WARN(wh, wa) warn({__FILE__, __LINE__}, log::Arg(wh, wa))
#define LOG_ERROR(wh, wa) error({__FILE__, __LINE__}, log::Arg(wh, wa))
#define LOG_FATAL(wh, wa) fatal({__FILE__, __LINE__}, log::Arg(wh, wa))
}
}

#endif // LOG_H
