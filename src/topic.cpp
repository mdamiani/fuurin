/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/topic.h"
#include "fuurin/zmqpartmulti.h"

#include <algorithm>
#include <cstring>


namespace fuurin {


Topic::Name::Name()
    : sz_(0)
{
    std::memset(dd_.data(), 0, dd_.size());
}


Topic::Name::Name(std::string_view str)
{
    sz_ = std::min(dd_.size(), str.size());
    std::memset(dd_.data(), 0, dd_.size());
    std::copy_n(str.data(), sz_, dd_.data());
}


Topic::Name::Name(const std::string& str)
    : Name(std::string_view(str.data(), str.size()))
{
}


constexpr size_t Topic::Name::capacity() const noexcept
{
    return dd_.size();
}


size_t Topic::Name::size() const noexcept
{
    return sz_;
}


bool Topic::Name::empty() const noexcept
{
    return sz_ == 0;
}


Topic::Name::operator std::string_view() const
{
    return std::string_view(dd_.data(), sz_);
}


Topic::Name::operator std::string() const
{
    return std::string(dd_.data(), sz_);
}


Topic::Topic()
{
}


Topic::Topic(const Uuid& broker, const Uuid& worker, const SeqN& seqn, const Name& name, const Data& data)
    : broker_(broker)
    , worker_(worker)
    , seqn_(seqn)
    , name_(name)
    , data_(data)
{
}


Topic::Topic(Uuid&& broker, Uuid&& worker, SeqN&& seqn, Name&& name, Data&& data)
    : broker_(broker)
    , worker_(worker)
    , seqn_(seqn)
    , name_(name)
    , data_(data)
{
}


Topic::~Topic() noexcept = default;


const Uuid& Topic::broker() const noexcept
{
    return broker_;
}


Uuid& Topic::broker() noexcept
{
    return broker_;
}


const Uuid& Topic::worker() const noexcept
{
    return worker_;
}


Uuid& Topic::worker() noexcept
{
    return worker_;
}


const Topic::SeqN& Topic::seqNum() const noexcept
{
    return seqn_;
}


Topic::SeqN& Topic::seqNum() noexcept
{
    return seqn_;
}


const Topic::Name& Topic::name() const noexcept
{
    return name_;
}


Topic::Name& Topic::name() noexcept
{
    return name_;
}


const Topic::Data& Topic::data() const noexcept
{
    return data_;
}


Topic::Data& Topic::data() noexcept
{
    return data_;
}


Topic& Topic::withBroker(const Uuid& v)
{
    broker_ = v;
    return *this;
}


Topic& Topic::withBroker(Uuid&& v)
{
    broker_ = std::move(v);
    return *this;
}


Topic& Topic::withWorker(const Uuid& v)
{
    worker_ = v;
    return *this;
}


Topic& Topic::withWorker(Uuid&& v)
{
    worker_ = std::move(v);
    return *this;
}


Topic& Topic::withSeqNum(const SeqN& v)
{
    seqn_ = v;
    return *this;
}


Topic& Topic::withSeqNum(SeqN&& v)
{
    seqn_ = std::move(v);
    return *this;
}


Topic& Topic::withName(const Name& v)
{
    name_ = v;
    return *this;
}


Topic& Topic::withName(Name&& v)
{
    name_ = std::move(v);
    return *this;
}


Topic& Topic::withData(const Data& v)
{
    data_ = v;
    return *this;
}


Topic& Topic::withData(Data&& v)
{
    data_ = std::move(v);
    return *this;
}


Topic Topic::fromPart(const zmq::Part& part)
{
    // TODO: unpack broker uuid
    // TODO: unpack worker uuid
    // TODO: unpack sequence number
    auto [name, data] = zmq::PartMulti::unpack<std::string_view, zmq::Part>(part);

    Topic t;
    t.name_ = name;
    t.data_ = data;

    return t;
}


zmq::Part Topic::toPart() const
{
    // TODO: pack broker uuid
    // TODO: pack worker uuid
    // TODO: pack sequence number
    return zmq::PartMulti::pack(std::string_view(name_), data_);
}


std::ostream& operator<<(std::ostream& os, const Topic& t)
{
    // TODO: output broker uuid
    // TODO: output worker uuid
    // TODO: output sequence number
    os << "[" << std::string_view(t.name()) << ", " << t.data().size() << "]";
    return os;
}

} // namespace fuurin
