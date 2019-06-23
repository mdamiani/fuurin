/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_WORKER_H
#define FUURIN_WORKER_H

#include "fuurin/topic.h"


namespace fuurin {


class Worker
{
    Worker();
    ~Worker() noexcept;

    void connect();
    void close() noexcept;
};

} // namespace fuurin

#endif // FUURIN_WORKER_H
