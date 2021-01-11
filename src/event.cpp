/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/event.h"
#include "fuurin/zmqpartmulti.h"
#include "failure.h"
#include "types.h"


using namespace std::literals::string_view_literals;


namespace fuurin {

Event::Event() noexcept
    : type_{Type::Invalid}
    , notif_{Notification::Discard}
{
}


Event::Event(Type type, Notification notif, const zmq::Part& data) noexcept
    : type_{type}
    , notif_{notif}
    , payld_{data}
{
}


Event::Event(Type type, Notification notif, zmq::Part&& data) noexcept
    : type_{type}
    , notif_{notif}
    , payld_{std::move(data)}
{
}


Event::~Event() noexcept = default;


Event::Type Event::type() const noexcept
{
    return type_;
}


Event::Notification Event::notification() const noexcept
{
    return notif_;
}


zmq::Part& Event::payload() noexcept
{
    return payld_;
}


const zmq::Part& Event::payload() const noexcept
{
    return payld_;
}


Event& Event::withType(Type v)
{
    type_ = v;
    return *this;
}


Event& Event::withNotification(Notification v)
{
    notif_ = v;
    return *this;
}


Event& Event::withPayload(const zmq::Part& v)
{
    payld_ = v;
    return *this;
}


Event& Event::withPayload(zmq::Part&& v)
{
    payld_ = std::move(v);
    return *this;
}


std::array<log::Arg, 3> Event::toArgs() const
{
    return {log::Arg{toString(type_)},
        log::Arg{toString(notif_)},
        log::Arg{int(payld_.size())}};
}


std::string_view Event::toString(Event::Notification v) noexcept
{
    switch (v) {
    case Event::Notification::Discard:
        return "discard"sv;
    case Event::Notification::Timeout:
        return "timeout"sv;
    case Event::Notification::Success:
        return "success"sv;
    case Event::Notification::COUNT:
        break;
    }
    return "n/a"sv;
}


std::string_view Event::toString(Event::Type v) noexcept
{
    switch (v) {
    case Event::Type::Invalid:
        return "invalid"sv;
    case Event::Type::Started:
        return "started"sv;
    case Event::Type::Stopped:
        return "stopped"sv;
    case Event::Type::Offline:
        return "offline"sv;
    case Event::Type::Online:
        return "online"sv;
    case Event::Type::Delivery:
        return "delivery"sv;
    case Event::Type::SyncRequest:
        return "sync/request"sv;
    case Event::Type::SyncBegin:
        return "sync/begin"sv;
    case Event::Type::SyncElement:
        return "sync/element"sv;
    case Event::Type::SyncSuccess:
        return "sync/success"sv;
    case Event::Type::SyncError:
        return "sync/error"sv;
    case Event::Type::SyncDownloadOn:
        return "sync/download/on"sv;
    case Event::Type::SyncDownloadOff:
        return "sync/download/off"sv;
    case Event::Type::COUNT:
        break;
    }
    return "n/a"sv;
}


Event Event::fromPart(const zmq::Part& part)
{
    return fromPart(part.toString());
}


Event Event::fromPart(std::string_view part)
{
    auto [type, notif, payload] = zmq::PartMulti::unpack<type_t, notif_t, zmq::Part>(part);

    ASSERT(type >= toIntegral(Event::Type::Invalid) &&
            type < toIntegral(Event::Type::COUNT),
        "Event::fromPart: bad operation type");

    ASSERT(notif >= toIntegral(Event::Notification::Discard) &&
            notif < toIntegral(Event::Notification::COUNT),
        "Event::fromPart: bad operation notification");

    return {Event::Type{type}, Event::Notification{notif}, std::move(payload)};
}


zmq::Part Event::toPart() const
{
    const type_t type = static_cast<type_t>(type_);
    const notif_t notif = static_cast<notif_t>(notif_);

    ASSERT(type >= toIntegral(Event::Type::Invalid) &&
            type < toIntegral(Event::Type::COUNT),
        "Event::toPart: bad operation type");

    ASSERT(notif >= toIntegral(Event::Notification::Discard) &&
            notif < toIntegral(Event::Notification::COUNT),
        "Event::toPart: bad operation notification");

    return zmq::PartMulti::pack(type, notif, payld_);
}


std::ostream& operator<<(std::ostream& os, const Event::Notification& v)
{
    os << Event::toString(v);
    return os;
}


std::ostream& operator<<(std::ostream& os, const Event::Type& v)
{
    os << Event::toString(v);
    return os;
}


std::ostream& operator<<(std::ostream& os, const Event& v)
{
    const auto& args = v.toArgs();
    log::printArgs(os, args.data(), args.size());
    return os;
}

} // namespace fuurin
