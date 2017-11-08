/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE common
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include "fuurin/fuurin.h"
#include "types.h"
#include "log.h"

#include <cstdio>
#include <string>

namespace bdata = boost::unit_test::data;


BOOST_AUTO_TEST_CASE(version)
{
    char buf[16];

    std::snprintf(buf, sizeof(buf), "%d.%d.%d",
        fuurin::VERSION_MAJOR,
        fuurin::VERSION_MINOR,
        fuurin::VERSION_PATCH);

    BOOST_TEST(std::string(fuurin::version()) == std::string(buf));
}


static std::string _myLogOutput;
static const char * _myLogPrefix[] = {
    "MY_DEBUG: ",
    "MY_INFO: ",
    "MY_WARN: ",
    "MY_ERROR: ",
    "MY_FATAL: ",
};

void myLogHandler(fuurin::LogLevel level, const char *file, unsigned int line, const std::string &message)
{
    UNUSED(file);
    UNUSED(line);

    _myLogOutput = _myLogPrefix[level] + message;
}

BOOST_DATA_TEST_CASE(customLogMessageHandler, bdata::make({
    fuurin::DebugLevel,
    fuurin::InfoLevel,
    fuurin::WarningLevel,
    fuurin::ErrorLevel,
    fuurin::FatalLevel,
}), level)
{
    const std::string msg = "message";

    fuurin::logInstallMessageHandler(myLogHandler);
    fuurin::logMessage(level, __FILE__, __LINE__, msg);

    BOOST_TEST(_myLogOutput == _myLogPrefix[level] + msg);
}
