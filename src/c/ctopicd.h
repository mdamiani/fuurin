/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_C_TOPIC_D_H
#define FUURIN_C_TOPIC_D_H


struct CTopic;

namespace fuurin {
namespace c {

/**
 * \return Pointer to the C structure.
 */
inline Topic* getPrivD(CTopic* p)
{
    return reinterpret_cast<Topic*>(p);
}


/**
 * \brief Pointer to the opaque structure.
 */
inline CTopic* getOpaque(Topic* p)
{
    return reinterpret_cast<CTopic*>(p);
}

} // namespace c
} // namespace fuurin

#endif // FUURIN_C_TOPIC_D_H
