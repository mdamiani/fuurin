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

namespace fuurin {

/**
 * \brief Library version as major, minor and patch components.
 */
///@{
constexpr const int VERSION_MAJOR = LIB_VERSION_MAJOR;
constexpr const int VERSION_MINOR = LIB_VERSION_MINOR;
constexpr const int VERSION_PATCH = LIB_VERSION_PATCH;
///@}


/**
 * \return Library version, formatted as string.
 */
char* version();
} // namespace fuurin

#endif // FUURIN_H
