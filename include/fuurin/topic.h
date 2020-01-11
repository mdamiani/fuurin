/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_TOPIC_H
#define FUURIN_TOPIC_H

#include "fuurin/uuid.h"
#include "fuurin/zmqpart.h"

#include <array>
#include <string>


namespace fuurin {

class Topic
{
public:
    using name_t = std::array<char, 16>;
    Topic(){};


private:
    Uuid broker_;
    Uuid worker_;
    uint64_t seqn_;
    name_t name_;
    zmq::Part data_;
};

} // namespace fuurin

#endif // FUURIN_TOPIC_H
