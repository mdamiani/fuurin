/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/stopwatch.h"


namespace fuurin {

StopWatch::StopWatch() noexcept
{
    start();
}


StopWatch::~StopWatch() noexcept = default;


void StopWatch::start() noexcept
{
    t0_ = std::chrono::steady_clock::now();
}


std::chrono::milliseconds StopWatch::elapsed() const noexcept
{
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - t0_);
}

} // namespace fuurin
