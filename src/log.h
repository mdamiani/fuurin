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

#include "fuurin/fuurin.h"


namespace fuurin {


/**
 * \brief Log message handler.
 *
 * This function is used to handle message logs.
 */
extern LogMessageHandler logMessage;


/**
 * \brief Simple formatting function for logs.
 *
 * \param[in] format String format, in printf-style.
 * \param[in] ... Variable number of arguments.
 *
 * \return A formatted string.
 */
std::string format(const char *format, ...);


#ifndef NDEBUG
#define LOG_DEBUG(msg)  logMessage(DebugLevel, msg)
#else
#define LOG_DEBUG(msg)
#endif

#define LOG_INFO(msg)   logMessage(InfoLevel, msg)
#define LOG_WARN(msg)   logMessage(WarningLevel, msg)
#define LOG_ERROR(msg)  logMessage(ErrorLevel, msg)
#define LOG_FATAL(msg)  logMessage(FatalLevel, msg)


}

#endif // LOG_H
