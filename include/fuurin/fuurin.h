/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_H
#define FUURIN_H

#include <fuurin/logger.h>

namespace fuurin {

/**
 * \brief Library version as major, minor and patch components.
 */
///{@
constexpr const int VERSION_MAJOR = 0;
constexpr const int VERSION_MINOR = 1;
constexpr const int VERSION_PATCH = 1;
///@}


/**
 * \return Library version, formatted as string.
 */
char* version();
}

#endif // FUURIN_H
