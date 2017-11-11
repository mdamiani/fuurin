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
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <string>

#include "fuurin.h"

BOOST_AUTO_TEST_CASE(version) {
    char buf[16];

    std::snprintf(buf, sizeof(buf), "%d.%d.%d",
        fuurin::VERSION_MAJOR,
        fuurin::VERSION_MINOR,
        fuurin::VERSION_PATCH);

    BOOST_CHECK_EQUAL(std::string(fuurin::version()), std::string(buf));
}
