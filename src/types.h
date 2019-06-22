/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TYPES_H
#define TYPES_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

#define UNUSED(x) (void)(x)

namespace fuurin {
using ByteArray = std::vector<uint8_t>;
using String = std::string;
} // namespace fuurin

#endif // TYPES_H
