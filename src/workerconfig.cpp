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
    return topicNames == rhs.topicNames &&
        seqNum == rhs.seqNum &&
        uuid == rhs.uuid;
}


bool WorkerConfig::operator!=(const WorkerConfig& rhs) const
{
    return !(*this == rhs);
}


WorkerConfig WorkerConfig::fromPart(const zmq::Part& part)
{
    WorkerConfig wc;

    const auto [uuid, seqNum, namesPart] = zmq::PartMulti::unpack<Uuid::Bytes, Topic::SeqN, zmq::Part>(part);

    std::vector<std::string_view> names;
    zmq::PartMulti::unpack(namesPart, std::inserter(names, names.begin()));

    wc.topicNames = std::vector<Topic::Name>(names.begin(), names.end());
    wc.seqNum = seqNum;
    wc.uuid = Uuid::fromBytes(uuid);

    return wc;
}


zmq::Part WorkerConfig::toPart() const
{
    const std::vector<std::string_view> names{topicNames.begin(), topicNames.end()};
    return zmq::PartMulti::pack(uuid.bytes(), seqNum, zmq::PartMulti::pack(names.begin(), names.end()));
}


std::ostream& operator<<(std::ostream& os, const WorkerConfig& wc)
{
    os << "[";
    os << wc.uuid << ", ";
    os << "[";
    const char* sep = "";
    for (const auto& el : wc.topicNames) {
        os << sep;
        os << el;
        sep = ", ";
    }
    os << "]";
    os << ", " << wc.seqNum;
    os << "]";

    return os;
}

} // namespace fuurin
