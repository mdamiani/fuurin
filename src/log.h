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
extern LogMessageHandler _logMessage;


#ifndef NDEBUG
#define LOG_DEBUG(msg) _logMessage(DebugLevel, msg)
#else
#define LOG_DEBUG(msg)
#endif

#define LOG_INFO(msg) _logMessage(InfoLevel, msg)
#define LOG_WARN(msg) _logMessage(WarningLevel, msg)
#define LOG_ERROR(msg) _logMessage(ErrorLevel, msg)
#define LOG_FATAL(msg) _logMessage(FatalLevel, msg)


}

#endif // LOG_H
