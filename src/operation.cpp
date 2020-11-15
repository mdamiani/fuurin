/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/operation.h"
#include "fuurin/zmqpartmulti.h"
#include "failure.h"
#include "types.h"


using namespace std::literals::string_view_literals;


namespace fuurin {

Operation::Operation() noexcept
    : type_{Type::Invalid}
    , notif_{Notification::Discard}
{
}


Operation::Operation(Type type, Notification notif, const zmq::Part& data) noexcept
    : type_{type}
    , notif_{notif}
    , payld_{data}
{
}


Operation::Operation(Type type, Notification notif, zmq::Part&& data) noexcept
    : type_{type}
    , notif_{notif}
    , payld_{std::move(data)}
{
}


Operation::~Operation() noexcept = default;


Operation::Type Operation::type() const noexcept
{
    return type_;
}


Operation::Notification Operation::notification() const noexcept
{
    return notif_;
}


zmq::Part& Operation::payload() noexcept
{
    return payld_;
}


const zmq::Part& Operation::payload() const noexcept
{
    return payld_;
}


Operation& Operation::withType(Type v)
{
    type_ = v;
    return *this;
}


Operation& Operation::withNotification(Notification v)
{
    notif_ = v;
    return *this;
}


Operation& Operation::withPayload(const zmq::Part& v)
{
    payld_ = v;
    return *this;
}


Operation& Operation::withPayload(zmq::Part&& v)
{
    payld_ = std::move(v);
    return *this;
}


std::array<log::Arg, 3> Operation::toArgs() const
{
    return {log::Arg{toString(type_)},
        log::Arg{toString(notif_)},
        log::Arg{int(payld_.size())}};
}


std::string_view Operation::toString(Operation::Notification v) noexcept
{
    switch (v) {
    case Operation::Notification::Discard:
        return "discard"sv;
    case Operation::Notification::Success:
        return "success"sv;
    case Operation::Notification::COUNT:
        break;
    }
    return "n/a"sv;
}


std::string_view Operation::toString(Operation::Type v) noexcept
{
    switch (v) {
    case Operation::Type::Invalid:
        return "invalid"sv;
    case Operation::Type::Start:
        return "start"sv;
    case Operation::Type::Stop:
        return "stop"sv;
    case Operation::Type::Dispatch:
        return "dispatch"sv;
    case Operation::Type::Sync:
        return "sync"sv;
    case Operation::Type::COUNT:
        break;
    }
    return "n/a"sv;
}


Operation Operation::fromPart(const zmq::Part& part)
{
    auto [type, notif, payload] = zmq::PartMulti::unpack<type_t, notif_t, zmq::Part>(part);

    ASSERT(type >= toIntegral(Operation::Type::Invalid) &&
            type < toIntegral(Operation::Type::COUNT),
        "Operation::fromPart: bad operation type");

    ASSERT(notif >= toIntegral(Operation::Notification::Discard) &&
            notif < toIntegral(Operation::Notification::COUNT),
        "Operation::fromPart: bad operation notification");

    return {Operation::Type{type}, Operation::Notification{notif}, std::move(payload)};
}


zmq::Part Operation::toPart() const
{
    const type_t type = static_cast<type_t>(type_);
    const notif_t notif = static_cast<notif_t>(notif_);

    ASSERT(type >= toIntegral(Operation::Type::Invalid) &&
            type < toIntegral(Operation::Type::COUNT),
        "Operation::toPart: bad operation type");

    ASSERT(notif >= toIntegral(Operation::Notification::Discard) &&
            notif < toIntegral(Operation::Notification::COUNT),
        "Operation::toPart: bad operation notification");

    return zmq::PartMulti::pack(type, notif, payld_);
}


std::ostream& operator<<(std::ostream& os, const Operation::Type& v)
{
    os << Operation::toString(v);
    return os;
}


std::ostream& operator<<(std::ostream& os, const Operation& v)
{
    const auto& args = v.toArgs();
    log::printArgs(os, args.data(), args.size());
    return os;
}

} // namespace fuurin
