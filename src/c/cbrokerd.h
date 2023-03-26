/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_C_BROKER_D_H
#define FUURIN_C_BROKER_D_H

#include "fuurin/broker.h"

#include <memory>
#include <future>


struct CBroker;

namespace fuurin {
namespace c {

/**
 * \brief Structure for Broker C bindings.
 */
struct CBrokerD
{
    std::unique_ptr<Broker> b; // Broker object.
    std::future<void> f;       // Broker future.
};


/**
 * \return Pointer to the C structure.
 */
inline CBrokerD* getPrivD(CBroker* p)
{
    return reinterpret_cast<CBrokerD*>(p);
}


/**
 * \brief Pointer to the opaque structure.
 */
inline CBroker* getOpaque(CBrokerD* p)
{
    return reinterpret_cast<CBroker*>(p);
}

} // namespace c
} // namespace fuurin

#endif // FUURIN_C_BROKER_D_H
