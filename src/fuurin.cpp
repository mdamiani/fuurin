/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/fuurin.h"

#include <cstdio>


namespace fuurin {

extern void logHandler(LogLevel, const char*, unsigned int, const std::string&);
extern LogMessageHandler logMessage;

static char _version_buf[16];

char* version()
{
    std::snprintf(_version_buf, sizeof(_version_buf), "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR,
        VERSION_PATCH);

    return _version_buf;
}


void logInstallMessageHandler(LogMessageHandler handler)
{
    if (handler)
        logMessage = handler;
    else
        logMessage = logHandler;
}
}
