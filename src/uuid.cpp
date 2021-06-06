/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/uuid.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/name_generator.hpp>

#include <string>
#include <algorithm>


using namespace std::literals::string_view_literals;


namespace {
inline boost::uuids::uuid uuidFromBytes(const fuurin::Uuid::Bytes& b)
{
    static_assert(fuurin::Uuid::Bytes{}.size() == boost::uuids::uuid::static_size());
    boost::uuids::uuid u;
    std::copy(b.begin(), b.end(), reinterpret_cast<char*>(&u));
    return u;
}


inline fuurin::Uuid::Bytes bytesFromUuid(const boost::uuids::uuid& u)
{
    fuurin::Uuid::Bytes b;
    const char* d = reinterpret_cast<const char*>(&u);
    std::copy(d, d + u.size(), b.begin());
    return b;
}


inline fuurin::Uuid::Srepr sreprFromBytes(const fuurin::Uuid::Bytes& b)
{
    static_assert(fuurin::Uuid::Srepr{}.size() == std::string_view(fuurin::Uuid::NullFmt).size());
    fuurin::Uuid::Srepr r;
    const auto& s = boost::uuids::to_string(uuidFromBytes(b));
    std::copy(s.begin(), s.end(), r.begin());
    return r;
}
} // namespace


namespace fuurin {

const Uuid Uuid::Ns::Dns{Uuid::fromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8"sv)};
const Uuid Uuid::Ns::Url{Uuid::fromString("6ba7b811-9dad-11d1-80b4-00c04fd430c8"sv)};
const Uuid Uuid::Ns::Oid{Uuid::fromString("6ba7b812-9dad-11d1-80b4-00c04fd430c8"sv)};
const Uuid Uuid::Ns::X500dn{Uuid::fromString("6ba7b814-9dad-11d1-80b4-00c04fd430c8"sv)};


Uuid::Uuid()
    : cached_{false}
{
    std::fill(bytes_.begin(), bytes_.end(), 0);
}


Uuid::Uuid(const Uuid& other)
{
    *this = other;
}


bool Uuid::isNull() const noexcept
{
    return uuidFromBytes(bytes_).is_nil();
}


const Uuid::Bytes& Uuid::bytes() const noexcept
{
    return bytes_;
}


std::string_view Uuid::toString() const
{
    cacheSrepr();
    return std::string_view(srepr_.data(), srepr_.size());
}


std::string_view Uuid::toShortString() const
{
    const size_t sz = 8;
    static_assert(sz <= NullFmt.size());
    cacheSrepr();
    return std::string_view(srepr_.data(), sz);
}


Uuid Uuid::fromString(std::string_view str)
{
    boost::uuids::string_generator gen;
    const auto& u = gen(str.begin(), str.end());
    return fromBytes(bytesFromUuid(u));
}


Uuid Uuid::fromString(const std::string& str)
{
    return fromString(std::string_view(str.data(), str.size()));
}


Uuid Uuid::createRandomUuid()
{
    boost::uuids::random_generator gen;
    const auto& u = gen();
    return fromBytes(bytesFromUuid(u));
}


Uuid Uuid::createNamespaceUuid(const Uuid& ns, std::string_view name)
{
    boost::uuids::name_generator_sha1 gen(uuidFromBytes(ns.bytes()));
    return fromBytes(bytesFromUuid(gen(name.data(), name.size())));
}


Uuid Uuid::createNamespaceUuid(const Uuid& ns, const std::string& name)
{
    return createNamespaceUuid(ns, std::string_view(name.data(), name.size()));
}


Uuid Uuid::fromBytes(const Bytes& b)
{
    Uuid ret;
    ret.bytes_ = b;
    return ret;
}


void Uuid::cacheSrepr() const
{
    const std::lock_guard<std::mutex> lock{reprMux_};

    if (cached_)
        return;

    srepr_ = sreprFromBytes(bytes_);
    cached_ = true;
}


Uuid Uuid::fromPart(const zmq::Part& part)
{
    return Uuid::fromBytes(std::get<0>(zmq::PartMulti::unpack<Uuid::Bytes>(part)));
}


zmq::Part Uuid::toPart() const
{
    return zmq::PartMulti::pack(bytes_);
}


Uuid& Uuid::operator=(const Uuid& rhs)
{
    std::lock_guard<std::mutex> lock{reprMux_};
    bytes_ = rhs.bytes_;
    cached_ = rhs.cached_;
    srepr_ = rhs.srepr_;
    return *this;
}


bool Uuid::operator==(const Uuid& rhs) const
{
    return bytes_ == rhs.bytes_;
}


bool Uuid::operator!=(const Uuid& rhs) const
{
    return !(*this == rhs);
}


std::ostream& operator<<(std::ostream& os, const Uuid& v)
{
    os << uuidFromBytes(v.bytes());
    return os;
}

} // namespace fuurin


namespace std {

size_t hash<fuurin::Uuid>::operator()(const fuurin::Uuid& u) const
{
    const auto& b = u.bytes();
    return std::hash<std::string_view>{}(std::string_view(reinterpret_cast<const char*>(b.data()), b.size()));
}

} // namespace std
