/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/workerconfig.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"

#include <iterator>


namespace fuurin {


bool WorkerConfig::operator==(const WorkerConfig& rhs) const
{
    return topicNames == rhs.topicNames;
}


bool WorkerConfig::operator!=(const WorkerConfig& rhs) const
{
    return !(*this == rhs);
}


WorkerConfig WorkerConfig::fromPart(const zmq::Part& part)
{
    WorkerConfig wc;

    std::vector<std::string_view> names;
    zmq::PartMulti::unpack(part, std::inserter(names, names.begin()));
    wc.topicNames = std::vector<Topic::Name>(names.begin(), names.end());

    return wc;
}


zmq::Part WorkerConfig::toPart() const
{
    const std::vector<std::string_view> names{topicNames.begin(), topicNames.end()};
    return zmq::PartMulti::pack(names.begin(), names.end());
}


std::ostream& operator<<(std::ostream& os, const WorkerConfig& wc)
{
    os << "[";
    os << "[";
    const char* sep = "";
    for (const auto& el : wc.topicNames) {
        os << sep;
        os << el;
        sep = ", ";
    }
    os << "]";
    os << "]";

    return os;
}

} // namespace fuurin
