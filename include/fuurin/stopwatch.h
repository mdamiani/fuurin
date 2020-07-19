/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_STOPWATCH_H
#define FUURIN_STOPWATCH_H

#include "fuurin/elapser.h"

#include <chrono>


namespace fuurin {

/**
 * \brief Time elapse counter.
 *
 * This class uses a monotonic clock to measure time.
 */
class StopWatch : public Elapser
{
public:
    /**
     * \brief Initializes the time reference to current time.
     */
    StopWatch() noexcept;

    /**
     * \brief Destructor.
     */
    virtual ~StopWatch() noexcept;

    /**
     * \brief Sets the time reference to current time.
     *
     * \see Elapser::start()
     */
    void start() noexcept override;

    /**
     * \brief Measure the elapsed time with respect to the reference time.
     *
     * \return Elapsed time in milliseconds.
     *
     * \see Elapser::elapsed()
     */
    std::chrono::milliseconds elapsed() const noexcept override;


private:
    std::chrono::steady_clock::time_point t0_; ///< Time reference.
};

} // namespace fuurin

#endif // FUURIN_STOPWATCH_H
