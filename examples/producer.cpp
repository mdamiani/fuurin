/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/broker.h"
#include "fuurin/worker.h"
#include "fuurin/zmqpart.h"

#include <future>
#include <thread>
#include <chrono>
#include <string_view>
#include <iostream>

using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;


int main()
{
    fuurin::Broker b;
    fuurin::Worker w;

    b.setEndpoints({"tcp://127.0.0.1:50101"}, {"tcp://127.0.0.1:50102"}, {"tcp://127.0.0.1:50103"});
    w.setEndpoints({"tcp://127.0.0.1:50101"}, {"tcp://127.0.0.1:50102"}, {"tcp://127.0.0.1:50103"});
    w.setTopicsNames({}); // just producer.

    auto f1 = b.start();
    auto f2 = w.start();
    auto f3 = std::async(std::launch::async, [&w]() { w.waitForStopped(); }); // shall consume events.

    for (uint8_t n = 1; n <= 10; ++n) {
        std::cout << "publish value " << std::to_string(n) << "\n";
        w.dispatch("/producer/value"sv, fuurin::zmq::Part{n});
        std::this_thread::sleep_for(1s);
    }

    b.stop();
    w.stop();

    f1.get();
    f2.get();
    f3.get();

    return 0;
}
