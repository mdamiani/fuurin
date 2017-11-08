/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_H
#define FUURIN_H

#include <string>


namespace fuurin {

/**
 * \brief Library version as major, minor and patch components.
 */
///{@
constexpr const int VERSION_MAJOR = 0;
constexpr const int VERSION_MINOR = 1;
constexpr const int VERSION_PATCH = 1;
///@}


/**
 * \return Library version, formatted as string.
 */
char * version();


/**
 * \brief Levels for logging messages.
 */
enum LogLevel {
    DebugLevel,         ///< Debug level.
    InfoLevel,          ///< Info level.
    WarningLevel,       ///< Warning level.
    ErrorLevel,         ///< Error level.
    FatalLevel,         ///< Fatal level.
};


/**
 * \brief Type for a callable log message handler function.
 *
 * \param[in] level Log level of the message.
 * \param[in] file  File name where the log happens.
 * \param[in] line  Code line where the log happens.
 */
typedef void (*LogMessageHandler)(LogLevel level, const char *file, unsigned int line, const std::string &message);


/**
 * \brief Installs a custom handler for library log messages.
 *
 * \param[in] handler A callable function of type \ref LogMessageHandler.
 *                    Passing NULL causes the default message handler to be installed.
 */
void logInstallMessageHandler(LogMessageHandler handler);


}

#endif // FUURIN_H
