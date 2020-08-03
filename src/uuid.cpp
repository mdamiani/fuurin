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

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/name_generator.hpp>

#include <string>
#include <algorithm>


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
    static_assert(fuurin::Uuid::Srepr{}.size() == std::string_view("hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh").size());
    fuurin::Uuid::Srepr r;
    const auto& s = boost::uuids::to_string(uuidFromBytes(b));
    std::copy(s.begin(), s.end(), r.begin());
    return r;
}
} // namespace


namespace fuurin {

Uuid::Uuid()
{
    std::fill(bytes_.begin(), bytes_.end(), 0);
    std::copy(Null.begin(), Null.end(), srepr_.begin());
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
    return std::string_view(srepr_.data(), srepr_.size());
}


std::string_view Uuid::toShortString() const
{
    const size_t sz = 8;
    static_assert(sz <= Null.size());
    return std::string_view(srepr_.data(), sz);
}


Uuid Uuid::fromString(std::string_view str)
{
    boost::uuids::string_generator gen;
    const auto& u = gen(str.begin(), str.end());
    return createFromBytes(bytesFromUuid(u));
}


Uuid Uuid::fromString(const std::string& str)
{
    return fromString(std::string_view(str.data(), str.size()));
}


Uuid Uuid::createRandomUuid()
{
    boost::uuids::random_generator gen;
    const auto& u = gen();
    return createFromBytes(bytesFromUuid(u));
}


Uuid Uuid::createNamespaceUuid(const Uuid& ns, std::string_view name)
{
    boost::uuids::name_generator_sha1 gen(uuidFromBytes(ns.bytes()));
    return createFromBytes(bytesFromUuid(gen(name.data(), name.size())));
}


Uuid Uuid::createNamespaceUuid(const Uuid& ns, const std::string& name)
{
    return createNamespaceUuid(ns, std::string_view(name.data(), name.size()));
}


Uuid Uuid::createFromBytes(const Bytes& b)
{
    Uuid ret;
    ret.bytes_ = b;
    ret.srepr_ = sreprFromBytes(b);
    return ret;
}


bool Uuid::operator==(const Uuid& rhs) const
{
    return bytes_ == rhs.bytes_ && srepr_ == rhs.srepr_;
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
