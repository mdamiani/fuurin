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


#ifndef NDEBUG
#define LOG_DEBUG(wh, wa) log::Logger::debug({__LINE__, __FILE__, wh, wa})
#else
#define LOG_DEBUG(wh, wa)
#endif

#define LOG_INFO(wh, wa) log::Logger::info({__LINE__, __FILE__, wh, wa})
#define LOG_WARN(wh, wa) log::Logger::warn({__LINE__, __FILE__, wh, wa})
#define LOG_ERROR(wh, wa) log::Logger::error({__LINE__, __FILE__, wh, wa})
#define LOG_FATAL(wh, wa) log::Logger::fatal({__LINE__, __FILE__, wh, wa})
}
}

#endif // LOG_H
