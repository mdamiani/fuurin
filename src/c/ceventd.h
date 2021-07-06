/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_C_EVENT_D_H
#define FUURIN_C_EVENT_D_H

#include "fuurin/event.h"
#include "fuurin/topic.h"


namespace {
struct CEventD
{
    fuurin::Event ev;
    fuurin::Topic tp;
};
} // namespace

#endif // FUURIN_C_EVENT_D_H
