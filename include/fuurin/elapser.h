/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_ELAPSER_H
#define FUURIN_ELAPSER_H

#include <chrono>


namespace fuurin {

/**
 * \brief Provides an interface to measure elapsed time.
 */
class Elapser
{
public:
    /**
     * \brief Destructor.
     */
    virtual ~Elapser() noexcept = default;

    /**
     * \brief Sets the time reference to current time.
     *
     * This is useful in order to measure the elapsed time,
     * with respect to the reference time.
     *
     * \see elapsed()
     */
    virtual void start() noexcept = 0;

    /**
     * \brief Measure the elapsed time with respect to the reference time.
     *
     * \return Elapsed time in milliseconds.
     *
     * \see start()
     */
    virtual std::chrono::milliseconds elapsed() const noexcept = 0;
};

} // namespace fuurin

#endif // FUURIN_ELAPSER_H
