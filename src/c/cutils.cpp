/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "cutils.h"

#include <tuple>
#include <algorithm>


namespace fuurin {
namespace c {

CUuid uuidConvert(const Uuid& id)
{
    CUuid ret;

    static_assert(std::tuple_size_v<fuurin::Uuid::Bytes> == sizeof(CUuid::bytes));
    std::copy_n(id.bytes().data(), id.size(), ret.bytes);

    return ret;
}


Uuid uuidConvert(const CUuid& id)
{
    Uuid::Bytes bytes;
    static_assert(bytes.size() == sizeof(id.bytes));

    std::copy_n(id.bytes, sizeof(id.bytes), bytes.begin());
    return Uuid::fromBytes(bytes);
}

} // namespace c
} // namespace fuurin
