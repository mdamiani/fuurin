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


using namespace std::literals::string_view_literals;


namespace fuurin {

Event::Event() noexcept
    : type_(Type::Invalid)
    , notif_(Notification::Discard)
{
}


Event::Event(Type type, Notification notif, zmq::Part&& data) noexcept
    : type_(type)
    , notif_(notif)
    , payld_(std::move(data))
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
    case Event::Type::COUNT:
        break;
    }
    return "n/a"sv;
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
