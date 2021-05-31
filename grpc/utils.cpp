/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "utils.h"
#include "fuurin/runner.h"

#include <vector>
#include <string>
#include <iostream>


namespace utils {

void parseArgsEndpoints(int argc, char** argv, int startIdx, fuurin::Runner* runner)
{
    std::vector<std::vector<std::string>> endp{3};

    for (int i = startIdx, k = 0; i < argc; ++i, (k = ++k % endp.size()))
        endp[k].push_back(std::string(argv[i]));

    if (argc > startIdx)
        runner->setEndpoints(endp[0], endp[1], endp[2]);

    std::cout << "Endpoints:\n";

    std::cout << "delivery: ";
    for (const auto& el : runner->endpointDelivery())
        std::cout << el << " ";
    std::cout << "\n";

    std::cout << "dispatch: ";
    for (const auto& el : runner->endpointDispatch())
        std::cout << el << " ";
    std::cout << "\n";

    std::cout << "snapshot: ";
    for (const auto& el : runner->endpointSnapshot())
        std::cout << el << " ";
    std::cout << "\n";
}

} // namespace utils
