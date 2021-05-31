/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


namespace fuurin {
class Runner;
}


namespace utils {

/**
 * \brief Parses endpoints starting at specified index of command line arguments.
 *
 * \param[in] argc Command line \c argc.
 * \param[in] argv Command line \c argv.
 * \param[in] startIdx Index of \c argv at which parsing is started.
 * \param[in] runner Runner to apply endpoints.
 */
void parseArgsEndpoints(int argc, char** argv, int startIdx, fuurin::Runner* runner);

} // namespace utils
