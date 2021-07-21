/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_C_WORKER_D_H
#define FUURIN_C_WORKER_D_H

#include "fuurin/worker.h"
#include "ceventd.h"

#include <memory>
#include <future>


namespace {

/**
 * \brief Opaque structure for Worker C bindings.
 */
struct CWorkerD
{
    std::unique_ptr<fuurin::Worker> w; // Worker object.
    std::future<void> f;               // Worker future.
    CEventD evd;                       // Last event received.
};

} // namespace


#endif // FUURIN_C_WORKER_D_H
