/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "failure.h"
#include "log.h"

#include <cstdlib>


namespace fuurin {

void failure(const char* file, unsigned int line, const char* expr, const char* what)
{
    log::fatal({file, line}, log::Arg("ASSERT", std::string_view(expr)), log::Arg("FAILURE", std::string_view(what)));
    std::abort();
}
} // namespace fuurin
