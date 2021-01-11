/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/broker.h"
#include "fuurin/brokerconfig.h"
#include "fuurin/sessionbroker.h"


namespace fuurin {

Broker::Broker(Uuid id, const std::string& name)
    : Runner(id, name)
{
}


Broker::~Broker() noexcept
{
}


zmq::Part Broker::prepareConfiguration() const
{
    return BrokerConfig{
        uuid(),
        endpointDelivery(),
        endpointDispatch(),
        endpointSnapshot(),
    }
        .toPart();
}


std::unique_ptr<Session> Broker::createSession() const
{
    return makeSession<BrokerSession>();
}


} // namespace fuurin
