/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/zmqpollable.h"

#include <algorithm>


namespace fuurin {
namespace zmq {

Pollable::Pollable() = default;
Pollable::~Pollable() noexcept = default;


void Pollable::registerPoller(PollerObserver* poller)
{
    if (std::find(observers_.begin(), observers_.end(), poller) != observers_.end())
        return;

    observers_.push_back(poller);
}


void Pollable::unregisterPoller(PollerObserver* poller)
{
    observers_.erase(std::remove(observers_.begin(), observers_.end(), poller), observers_.end());
}


size_t Pollable::pollersCount() const noexcept
{
    return observers_.size();
}

} // namespace zmq
} // namespace fuurin
