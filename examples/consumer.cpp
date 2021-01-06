/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/worker.h"
#include "fuurin/topic.h"

#include <string_view>
#include <iostream>


int main()
{
    fuurin::Worker w;

    w.setEndpoints({"tcp://127.0.0.1:50101"}, {"tcp://127.0.0.1:50102"}, {"tcp://127.0.0.1:50103"});

    auto f = w.start();

    for (;;) {
        auto t = w.waitForTopic().value();
        std::cout << t << "\n";
        if (t.data().toUint8() >= 10)
            break;
    }

    w.stop();
    f.get();

    return 0;
}
