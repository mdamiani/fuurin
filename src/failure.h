/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FAILURE_H
#define FAILURE_H

#include <boost/config.hpp> // for BOOST_LIKELY

#include <string>


namespace fuurin {

/**
 * \brief Causes a program abort because of an assertion failure.
 *
 * \param[in] message Assertion message.
 * \param[in] file File name.
 * \param[in] line Code line.
 * \param[in] expr Expression which evaluated to \c false and caused the failure.
 */
void failure(const char* file, unsigned int line, const char* expr, const char* message);


/**
 * \brief Runtime assertion with message.
 *
 * \param[in] expr Assertion expression to evaluate.
 * \param[in] msg  Assertion message.
 */
#define ASSERT(expr, msg) \
    if (BOOST_UNLIKELY(!(expr))) { \
        failure(__FILE__, __LINE__, #expr, msg); \
    }
} // namespace fuurin


#endif // FAILURE_H
