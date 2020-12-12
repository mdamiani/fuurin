/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/brokerconfig.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"

#include <iterator>


namespace fuurin {


bool BrokerConfig::operator==(const BrokerConfig& rhs) const
{
    return uuid == rhs.uuid &&
        endpDelivery == rhs.endpDelivery &&
        endpDispatch == rhs.endpDispatch &&
        endpSnapshot == rhs.endpSnapshot;
}


bool BrokerConfig::operator!=(const BrokerConfig& rhs) const
{
    return !(*this == rhs);
}


BrokerConfig BrokerConfig::fromPart(const zmq::Part& part)
{
    BrokerConfig cc;

    const auto [uuid, endp1, endp2, endp3] = zmq::PartMulti::unpack<
        Uuid::Bytes,
        zmq::Part,
        zmq::Part,
        zmq::Part>(part);

    cc.uuid = Uuid::fromBytes(uuid);

    zmq::PartMulti::unpack(endp1, std::inserter(cc.endpDelivery, cc.endpDelivery.begin()));
    zmq::PartMulti::unpack(endp2, std::inserter(cc.endpDispatch, cc.endpDispatch.begin()));
    zmq::PartMulti::unpack(endp3, std::inserter(cc.endpSnapshot, cc.endpSnapshot.begin()));

    return cc;
}


zmq::Part BrokerConfig::toPart() const
{
    return zmq::PartMulti::pack(uuid.bytes(),
        zmq::PartMulti::pack(endpDelivery.begin(), endpDelivery.end()),
        zmq::PartMulti::pack(endpDispatch.begin(), endpDispatch.end()),
        zmq::PartMulti::pack(endpSnapshot.begin(), endpSnapshot.end()));
}


std::ostream& operator<<(std::ostream& os, const BrokerConfig& cc)
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
    os << cc.uuid << ", ";
    putList(cc.endpDelivery) << ", ";
    putList(cc.endpDispatch) << ", ";
    putList(cc.endpSnapshot);
    os << "]";

    return os;
}

} // namespace fuurin
