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
#include <iostream>
#include <sstream>

namespace bdata = boost::unit_test::data;


BOOST_AUTO_TEST_CASE(version)
{
    char buf[16];

    std::snprintf(buf, sizeof(buf), "%d.%d.%d", fuurin::VERSION_MAJOR, fuurin::VERSION_MINOR,
        fuurin::VERSION_PATCH);

    BOOST_TEST(std::string(fuurin::version()) == std::string(buf));
}


struct redirect
{
    redirect(std::ostream& oss, std::streambuf* buf)
        : _oss(oss)
        , _old(oss.rdbuf(buf))
    {}

    ~redirect()
    {
        _oss.rdbuf(_old);
    }

private:
    std::ostream& _oss;
    std::streambuf* _old;
};


BOOST_DATA_TEST_CASE(standardLogMessageHandler,
    bdata::make({
        fuurin::log::Logger::debug,
        fuurin::log::Logger::info,
        fuurin::log::Logger::warn,
        fuurin::log::Logger::error,
    }),
    logfn)
{
    std::stringstream buf;
    const auto& ro = redirect(std::cout, buf.rdbuf());
    const auto& re = redirect(std::cerr, buf.rdbuf());

    fuurin::log::Logger::installMessageHandler(new fuurin::log::StandardHandler);

    const fuurin::log::Message msg{1, "test_file", "test_fun", "test_msg"};
    const auto expected = std::string(msg.where) + ": " + msg.what;

    logfn(msg);

    auto out = buf.str();
    for (const char& c : {'\r', '\n'})
        out.erase(std::remove(out.begin(), out.end(), c), out.end());

    BOOST_TEST(out == expected);
}


BOOST_DATA_TEST_CASE(silentLogMessageHandler,
    bdata::make({
        fuurin::log::Logger::debug,
        fuurin::log::Logger::info,
        fuurin::log::Logger::warn,
        fuurin::log::Logger::error,
    }),
    logfn)
{
    std::stringstream buf;
    const auto ro = redirect(std::cout, buf.rdbuf());
    const auto re = redirect(std::cerr, buf.rdbuf());

    fuurin::log::Logger::installMessageHandler(new fuurin::log::SilentHandler);

    logfn({1, "test_file", "test_fun", "test_msg"});

    BOOST_TEST(buf.str().empty());
}


BOOST_AUTO_TEST_CASE(formatLog)
{
    BOOST_TEST("test_fun: test_msg1" == fuurin::log::format("test_fun: %s%d", "test_msg", 1));
}
