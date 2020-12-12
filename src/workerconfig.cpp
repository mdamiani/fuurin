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
    return uuid == rhs.uuid &&
        seqNum == rhs.seqNum &&
        topicNames == rhs.topicNames &&
        endpDelivery == rhs.endpDelivery &&
        endpDispatch == rhs.endpDispatch &&
        endpSnapshot == rhs.endpSnapshot;
}


bool WorkerConfig::operator!=(const WorkerConfig& rhs) const
{
    return !(*this == rhs);
}


WorkerConfig WorkerConfig::fromPart(const zmq::Part& part)
{
    WorkerConfig wc;

    const auto [uuid, seqNum, subscr, endp1, endp2, endp3] = zmq::PartMulti::unpack<
        Uuid::Bytes,
        Topic::SeqN,
        zmq::Part,
        zmq::Part,
        zmq::Part,
        zmq::Part>(part);

    std::vector<std::string_view> names;
    zmq::PartMulti::unpack(subscr, std::inserter(names, names.begin()));

    wc.uuid = Uuid::fromBytes(uuid);
    wc.seqNum = seqNum;
    wc.topicNames = std::vector<Topic::Name>(names.begin(), names.end());

    zmq::PartMulti::unpack(endp1, std::inserter(wc.endpDelivery, wc.endpDelivery.begin()));
    zmq::PartMulti::unpack(endp2, std::inserter(wc.endpDispatch, wc.endpDispatch.begin()));
    zmq::PartMulti::unpack(endp3, std::inserter(wc.endpSnapshot, wc.endpSnapshot.begin()));

    return wc;
}


zmq::Part WorkerConfig::toPart() const
{
    const std::vector<std::string_view> names{topicNames.begin(), topicNames.end()};

    return zmq::PartMulti::pack(uuid.bytes(), seqNum,
        zmq::PartMulti::pack(names.begin(), names.end()),
        zmq::PartMulti::pack(endpDelivery.begin(), endpDelivery.end()),
        zmq::PartMulti::pack(endpDispatch.begin(), endpDispatch.end()),
        zmq::PartMulti::pack(endpSnapshot.begin(), endpSnapshot.end()));
}


std::ostream& operator<<(std::ostream& os, const WorkerConfig& wc)
{
    auto putList = [&os](const auto& container) -> std::ostream& {
        os << "[";
        const char* sep = "";
        for (const auto& el : container) {
            os << sep;
            os << el;
            sep = ", ";
        }
        os << "]";
        return os;
    };

    os << "[";
    os << wc.uuid << ", ";
    os << wc.seqNum << ", ";
    putList(wc.topicNames) << ", ";
    putList(wc.endpDelivery) << ", ";
    putList(wc.endpDispatch) << ", ";
    putList(wc.endpSnapshot);
    os << "]";

    return os;
}

} // namespace fuurin
