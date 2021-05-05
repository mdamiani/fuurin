/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE tokenpool
#include <boost/test/unit_test.hpp>

#include "fuurin/tokenpool.h"

#include <string_view>
#include <algorithm>
#include <thread>
#include <future>


using namespace fuurin;
namespace utf = boost::unit_test;


BOOST_AUTO_TEST_CASE(tokenPoolPut)
{
    const int N = 32;

    TokenPool tp;
    std::future<void> fg[N];
    int vv[N];

    std::fill(std::begin(vv), std::end(vv), 0);

    for (int i = 0; i < N; ++i) {
        fg[i] = std::async(std::launch::async, [&tp, &vv]() {
            const auto id = tp.get();
            ++vv[id - 1];
        });
    }

    auto fp = std::async(std::launch::async, [&tp]() {
        for (TokenPool::id_t id = 1; id <= N; ++id)
            tp.put(id);
    });

    fp.get();
    for (auto& e : fg)
        e.get();

    for (auto v : vv)
        BOOST_TEST(v == 1);
}


BOOST_AUTO_TEST_CASE(tokenPoolGet)
{
    const int N = 32;

    TokenPool tp{1, N};
    int vv[N];

    std::fill(std::begin(vv), std::end(vv), 1);

    for (;;) {
        auto id = tp.tryGet();
        if (!id)
            break;

        --vv[id.value() - 1];
    }

    for (auto v : vv)
        BOOST_TEST(v == 0);
}
