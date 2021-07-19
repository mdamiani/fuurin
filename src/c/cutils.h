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

#include "fuurin/errors.h"
#include "fuurin/uuid.h"
#include "fuurin/c/cuuid.h"

#include <string_view>
#include <type_traits>


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


/**
 * \brief Logs an error.
 *
 * \param[in] e Excpetion or message to log.
 */
///@{
void logError(std::string_view e) noexcept;
void logError(const std::exception& e) noexcept;
void logError(const err::Error& e) noexcept;
///@}


/**
 * \brief Logs an exception using a Lippincott function.
 */
void logError() noexcept;


/**
 * \brief Calls a functions catching its exceptions.
 *
 * The function of type \c F is called and returned.
 * In case of any thrown exceptions from calling \c F,
 * the function of type \c C is called and returned.
 *
 * The return type of \c F and \c C must be the same.
 *
 * \param[f] Function to call and return, may throw an expection.
 * \param[c] Function fo call and return, in case of exceptions.
 *
 * \return Either \c f or \c c return value.
 *
 * \see logError()
 */
template<typename F, typename C>
auto withCatch(F&& f, C&& c) noexcept -> decltype(f())
{
    static_assert(std::is_same_v<decltype(f()), decltype(c())>);
    try {
        return f();
    } catch (...) {
        logError();
        return c();
    }
}

} // namespace c
} // namespace fuurin

#endif // FUURIN_C_UTILS_H
