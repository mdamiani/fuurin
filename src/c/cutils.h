/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_C_UTILS_H
#define FUURIN_C_UTILS_H

#include "fuurin/uuid.h"
#include "fuurin/c/cuuid.h"


namespace fuurin {
namespace c {

/**
 * \brief Converts Uuid from C++ to C.
 * \param[in] id A C++ Uuid.
 * \return A C Uuid.
 */
CUuid uuidConvert(const Uuid& id);


/**
 * \brief Converts Uuid from C to C++.
 * \param[in] id A C Uuid.
 * \return A C++ Uuid.
 */
Uuid uuidConvert(const CUuid& id);

} // namespace c
} // namespace fuurin

#endif // FUURIN_C_UTILS_H
