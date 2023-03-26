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


struct CEvent;

namespace fuurin {
namespace c {

/**
 * \brief Structure for Worker C bindings.
 */
struct CEventD
{
    Event ev; // C++ event.
    Topic tp; // C++ topic.
};


/**
 * \return Pointer to the C structure.
 */
inline CEventD* getPrivD(CEvent* p)
{
    return reinterpret_cast<CEventD*>(p);
}


/**
 * \brief Pointer to the opaque structure.
 */
inline CEvent* getOpaque(CEventD* p)
{
    return reinterpret_cast<CEvent*>(p);
}

} // namespace c
} // namespace fuurin

#endif // FUURIN_C_EVENT_D_H
