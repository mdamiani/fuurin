/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <functional>


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

/// Type for a callable log handler function.
typedef std::function<void(const std::string &)> LogHandlerFn;

/**
 * \brief Installs log handler for library log messages.
 * \param[in] fn A callable function of type \ref LogHandlerFn.
 */
///{@
void logInstallDebugHandler(const LogHandlerFn &fn);
void logInstallInfoHandler(const LogHandlerFn &fn);
void logInstallWarnHandler(const LogHandlerFn &fn);
void logInstallFatalHandler(const LogHandlerFn &fn);
///@}

/**
 * \brief Installs default handlers for library log messages.
 */
void logInstallDefaultHandlers();

}
