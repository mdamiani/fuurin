/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin_worker_impl.h"
#include "utils.h"


int main(int argc, char** argv)
{
    auto serverAddr = utils::parseArgsServerAddress(argc, argv, 1);

    auto [service, future, cancel, endpts] = WorkerServiceImpl::Run(serverAddr,
        utils::parseArgsEndpoints(argc, argv, 2));

    utils::printArgsServerAddress(serverAddr);
    utils::printArgsEndpoints(endpts);

    future.get();
    cancel();

    return 0;
}
