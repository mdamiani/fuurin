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
#include "fuurin/errors.h"
#include "fuurin/zmqpartmulti.h"

#include <zmq.h> // ZMQ_GROUP_MAX_LENGTH

#include <string_view>
#include <algorithm>
#include <cstring>


using namespace std::literals::string_view_literals;


namespace fuurin {


Topic::Name::Name()
    : sz_(0)
{
    static_assert(std::tuple_size<decltype(dd_)>::value <= ZMQ_GROUP_MAX_LENGTH + 1,
        "topic name exceeds ZMQ_GROUP_MAX_LENGTH");

    std::memset(dd_.data(), 0, dd_.size());
}


Topic::Name::Name(std::string_view str)
{
    sz_ = std::min(capacity(), str.size());
    std::memset(dd_.data(), 0, dd_.size());
    std::copy_n(str.data(), sz_, dd_.data());
}


Topic::Name::Name(const std::string& str)
    : Name(std::string_view(str.data(), str.size()))
{
}


constexpr size_t Topic::Name::capacity() const noexcept
{
    return dd_.size() - 1;
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


bool Topic::Name::operator==(const Name& rhs) const
{
    return sz_ == rhs.sz_ && dd_ == rhs.dd_;
}


bool Topic::Name::operator!=(const Name& rhs) const
{
    return !(*this == rhs);
}


Topic::Topic()
    : seqn_{0}
{
}


Topic::Topic(const Uuid& broker, const Uuid& worker, const SeqN& seqn, const Name& name, const Data& data)
    : broker_{broker}
    , worker_{worker}
    , seqn_{seqn}
    , name_{name}
    , data_{data}
{
}


Topic::Topic(Uuid&& broker, Uuid&& worker, SeqN&& seqn, Name&& name, Data&& data)
    : broker_{broker}
    , worker_{worker}
    , seqn_{seqn}
    , name_{name}
    , data_{data}
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


bool Topic::operator==(const Topic& rhs) const
{
    return broker_ == rhs.broker_ &&
        worker_ == rhs.worker_ &&
        seqn_ == rhs.seqn_ &&
        name_ == rhs.name_ &&
        data_ == rhs.data_;
}


bool Topic::operator!=(const Topic& rhs) const
{
    return !(*this == rhs);
}


Topic Topic::fromPart(const zmq::Part& part)
{
    auto [seqn, brok, work, name, data] = zmq::PartMulti::unpack<Topic::SeqN,
        Uuid::Bytes, Uuid::Bytes, std::string_view, zmq::Part>(part);

    return Topic{Uuid::fromBytes(brok), Uuid::fromBytes(work),
        std::move(seqn), std::move(name), std::move(data)};
}


zmq::Part Topic::toPart() const
{
    return zmq::PartMulti::pack(seqn_, broker_.bytes(), worker_.bytes(), std::string_view(name_), data_);
}


zmq::Part& Topic::withSeqNum(zmq::Part& part, Topic::SeqN val)
{
    const zmq::Part buf{val};

    if (part.size() < buf.size()) {
        throw ERROR(ZMQPartAccessFailed, "could not access topic multi part seqn field",
            log::Arg{std::string_view("reason"), "out of bound access"sv});
    }

    std::copy_n(buf.data(), buf.size(), part.data());
    return part;
}


std::ostream& operator<<(std::ostream& os, const Topic::Name& n)
{
    os << std::string_view(n);
    return os;
}


std::ostream& operator<<(std::ostream& os, const Topic& t)
{
    os << "["
       << t.broker() << ", "
       << t.worker() << ", "
       << t.seqNum() << ", "
       << t.name() << ", "
       << t.data().size()
       << "]";

    return os;
}

} // namespace fuurin


namespace std {

size_t hash<fuurin::Topic::Name>::operator()(const fuurin::Topic::Name& n) const
{
    return std::hash<std::string_view>{}(n);
}

} // namespace std
