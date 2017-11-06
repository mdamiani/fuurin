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

#include <cstdlib>
#include <cstdio>
#include <iostream>


namespace fuurin {


void logMessage(LogLevel level, const std::string &message)
{
    switch(level) {
    case DebugLevel:
        std::cout << "DEBUG: " << message << std::endl;
        break;

    case InfoLevel:
        std::cout << "INFO: " << message << std::endl;
        break;

    case WarningLevel:
        std::cerr << "WARN: " << message << std::endl;
        break;

    case ErrorLevel:
        std::cerr << "ERROR: " << message << std::endl;
        break;

    case FatalLevel:
        std::cerr << "FATAL: " << message << std::endl;
        std::abort();
        break;
    }
}


LogMessageHandler _logMessage = logMessage;


}
