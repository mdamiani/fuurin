/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_UUID_H
#define FUURIN_UUID_H

#include <array>


namespace fuurin {

// TODO: to be implemented.
struct Uuid
{
    std::array<uint8_t, 16> data;
};

} // namespace fuurin

#endif // FUURIN_UUID_H
