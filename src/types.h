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

#include <type_traits>
#include <chrono>
#include <limits>


#define UNUSED(x) (void)(x)


namespace fuurin {
namespace zmq {

template<typename T>
std::enable_if_t<std::is_integral_v<std::decay_t<T>>, T>
getMillis(std::chrono::milliseconds val)
{
    if (val.count() < 0) {
        return -1;
    } else if (val.count() > std::numeric_limits<T>::max()) {
        return std::numeric_limits<T>::max();
    } else {
        return static_cast<T>(val.count());
    }
}

} // namespace zmq

template<typename Enum>
constexpr std::underlying_type_t<Enum> toIntegral(Enum e)
{
    return static_cast<std::underlying_type_t<Enum>>(e);
}

} // namespace fuurin

#endif // TYPES_H
