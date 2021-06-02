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
    auto [service, future, cancel, endpts] = WorkerServiceImpl::Run("localhost:50051",
        utils::parseArgsEndpoints(argc, argv, 1));

    utils::printArgsEndpoints(endpts);

    future.get();
    cancel();

    return 0;
}
