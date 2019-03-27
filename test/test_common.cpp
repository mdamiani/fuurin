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

    std::snprintf(buf, sizeof(buf), "%d.%d.%d", fuurin::VERSION_MAJOR, fuurin::VERSION_MINOR,
        fuurin::VERSION_PATCH);

    BOOST_TEST(std::string(fuurin::version()) == std::string(buf));
}


static std::string _myLogOutput;
static std::string _myLogPrefix[] = {
    "MY_DEBUG: ",
    "MY_INFO: ",
    "MY_WARN: ",
    "MY_ERROR: ",
    "MY_FATAL: ",
};

class MyLogHandler : public fuurin::log::Handler
{
public:
    void debug(const fuurin::log::Message& m)
    {
        _myLogOutput = _myLogPrefix[0] + m.what;
    }

    void info(const fuurin::log::Message& m)
    {
        _myLogOutput = _myLogPrefix[1] + m.what;
    }

    void warn(const fuurin::log::Message& m)
    {
        _myLogOutput = _myLogPrefix[2] + m.what;
    }

    void error(const fuurin::log::Message& m)
    {
        _myLogOutput = _myLogPrefix[3] + m.what;
    }

    void fatal(const fuurin::log::Message& m)
    {
        _myLogOutput = _myLogPrefix[4] + m.what;
    }
};

BOOST_DATA_TEST_CASE(customLogMessageHandler,
    bdata::make({
        std::make_tuple(fuurin::log::Logger::debug, 0),
        std::make_tuple(fuurin::log::Logger::info, 1),
        std::make_tuple(fuurin::log::Logger::warn, 2),
        std::make_tuple(fuurin::log::Logger::error, 3),
        std::make_tuple(fuurin::log::Logger::fatal, 4),
    }),
    logfn, level)
{
    const char* msg = "message";
    fuurin::log::Logger::installMessageHandler(new MyLogHandler);
    logfn({__LINE__, __FILE__, "customLogMessageHandler", msg});

    BOOST_TEST(_myLogOutput == _myLogPrefix[level] + msg);
}
