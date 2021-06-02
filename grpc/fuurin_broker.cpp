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
#include "utils.h"


int main(int argc, char** argv)
{
    fuurin::Broker broker;

    auto endpts = utils::parseAndApplyArgsEndpoints(argc, argv, 1, &broker);

    utils::printArgsEndpoints(endpts);

    auto bf = broker.start();

    utils::waitForResult(&bf);

    return 0;
}
