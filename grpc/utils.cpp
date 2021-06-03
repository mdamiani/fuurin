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
#include "fuurin/errors.h"
#include "fuurin/logger.h"

#include <string_view>
#include <iostream>


using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;

namespace flog = fuurin::log;

namespace utils {


Endpoints parseAndApplyArgsEndpoints(int argc, char** argv, int startIdx, fuurin::Runner* runner)
{
    return applyArgsEndpoints(parseArgsEndpoints(argc, argv, startIdx), runner);
}


Endpoints parseArgsEndpoints(int argc, char** argv, int startIdx)
{
    std::vector<std::vector<std::string>> endp{3};

    for (int i = startIdx, k = 0; i < argc; ++i, (k + 1 >= int(endp.size()) ? k = 0 : ++k))
        endp[k].push_back(std::string(argv[i]));

    return {endp[0], endp[1], endp[2]};
}


std::string parseArgsServerAddress(int argc, char** argv, int startIdx)
{
    if (startIdx >= argc)
        return "localhost:50051";

    return argv[startIdx];
}


Endpoints applyArgsEndpoints(Endpoints endpts, fuurin::Runner* runner)
{
    if (!endpts.delivery.empty() || !endpts.dispatch.empty() || !endpts.snapshot.empty())
        runner->setEndpoints(endpts.delivery, endpts.dispatch, endpts.snapshot);

    return {
        runner->endpointDelivery(),
        runner->endpointDispatch(),
        runner->endpointSnapshot(),
    };
}


void printArgsEndpoints(const Endpoints& endpts)
{
    std::cout << "Endpoints:\n";

    std::cout << "delivery: ";
    for (const auto& el : endpts.delivery)
        std::cout << el << " ";
    std::cout << "\n";

    std::cout << "dispatch: ";
    for (const auto& el : endpts.dispatch)
        std::cout << el << " ";
    std::cout << "\n";

    std::cout << "snapshot: ";
    for (const auto& el : endpts.snapshot)
        std::cout << el << " ";
    std::cout << "\n";
}


void printArgsServerAddress(const std::string& addr)
{
    std::cout << "Server address: " << addr << "\n";
}


void waitForResult(std::future<void>* future, std::chrono::milliseconds timeout)
{
    if (!future->valid())
        return;

    if (timeout >= 0ms) {
        if (future->wait_for(timeout) != std::future_status::ready)
            return;
    }

    try {
        future->get();
    } catch (const fuurin::err::Error& e) {
        // TODO: use logger helper functions
        flog::Arg args[] = {flog::Arg{"error"sv, std::string_view(e.what())}, e.arg()};
        fuurin::log::Logger::error(e.loc(), args, 2);
    }
}

} // namespace utils
