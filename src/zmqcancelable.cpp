/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/zmqcancelable.h"
#include "fuurin/zmqtimer.h"


using namespace std::chrono_literals;

namespace fuurin {
namespace zmq {


Cancelable::Cancelable(Context* ctx, const std::string& name)
    : timer_{std::make_unique<Timer>(ctx, name)}
{
    timer_->setSingleShot(true);
}


Cancelable::~Cancelable() noexcept
{
    cancel();
}


void* Cancelable::zmqPointer() const noexcept
{
    return timer_->zmqPointer();
}


bool Cancelable::isOpen() const noexcept
{
    return timer_->isOpen();
}


std::string Cancelable::description() const
{
    return timer_->description();
}


Cancelable& Cancelable::withDeadline(std::chrono::milliseconds timeout)
{
    setDeadline(timeout);
    return *this;
}


void Cancelable::setDeadline(std::chrono::milliseconds timeout)
{
    if (timeout < 0ms)
        return;

    timer_->setInterval(timeout);
    timer_->start();
}


void Cancelable::cancel()
{
    timer_->setInterval(0ms);
    timer_->start();
}


bool Cancelable::isCanceled()
{
    return timer_->isExpired();
}


} // namespace zmq
} // namespace fuurin
