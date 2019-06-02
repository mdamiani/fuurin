/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/logger.h"
#include "failure.h"

#include <zmq.h>
#include <boost/preprocessor/seq/for_each.hpp>

#include <cstring>
#include <cstdlib>
#include <iostream>


namespace fuurin {
namespace log {

/**
 * Arg
 */

Arg::Val::Val(int v) noexcept
    : int_(v)
{
}


Arg::Val::Val(double v) noexcept
    : dbl_(v)
{
}


Arg::Val::Val(std::string_view v) noexcept
    : chr_(v)
{
}


Arg::Val::Val(Ref<char>* v) noexcept
    : str_(v)
{
}


Arg::Val::Val(Ref<Arg>* v) noexcept
    : arr_(v)
{
}


Arg::Arg() noexcept
    : type_(Type::Invalid)
    , alloc_(Alloc::None)
    , key_(nullptr)
    , val_(0)
{
}


Arg::Arg(int val) noexcept
    : Arg(std::string_view(), val)
{
}


Arg::Arg(ec_t val) noexcept
    : Arg(std::string_view(), val)
{
}


Arg::Arg(double val) noexcept
    : Arg(std::string_view(), val)
{
}


Arg::Arg(std::string_view val) noexcept
    : Arg(std::string_view(), val)
{
}


Arg::Arg(const std::string& val)
    : Arg(std::string_view(), val)
{
}


Arg::Arg(std::initializer_list<Arg> l)
    : Arg(std::string_view(), l)
{
}


Arg::Arg(std::string_view key, int val) noexcept
    : type_(Type::Int)
    , alloc_(Alloc::None)
    , key_(key)
    , val_(val)
{
}


Arg::Arg(std::string_view key, ec_t val) noexcept
    : type_(Type::Errno)
    , alloc_(Alloc::None)
    , key_(key)
    , val_(val.val)
{
}


Arg::Arg(std::string_view key, double val) noexcept
    : type_(Type::Double)
    , alloc_(Alloc::None)
    , key_(key)
    , val_(val)
{
}


Arg::Arg(std::string_view key, std::string_view val) noexcept
    : type_(Type::String)
    , alloc_(Alloc::None)
    , key_(key)
    , val_(val)
{
}


Arg::Arg(std::string_view key, const std::string& val)
    : type_(Type::String)
    , alloc_(Alloc::None)
    , key_(key)
    , val_(0)
{
    const size_t sz = val.size();

    if (sz <= MaxStringStackSize) {
        alloc_ = Alloc::Stack;
        std::memcpy(val_.buf_.dat_, val.data(), sz);
        val_.buf_.siz_ = sz;
    } else {
        alloc_ = Alloc::Heap;
        val_.str_ = new Ref<char>(sz);
        ++val_.str_->cnt_;
        std::memcpy(val_.str_->buf_, val.data(), sz);
    }
}


Arg::Arg(std::string_view key, const Arg val[], size_t size)
    : type_(Type::Array)
    , alloc_(Alloc::Heap)
    , key_(key)
    , val_(new Ref<Arg>(size))
{
    ++val_.arr_->cnt_;
    for (size_t i = 0; i < size; ++i)
        val_.arr_->buf_[i] = val[i];
}


Arg::Arg(std::string_view key, std::initializer_list<Arg> l)
    : Arg(key, &*l.begin(), l.size())
{
}


Arg::Arg(const Arg& other) noexcept
    : type_(other.type_)
    , alloc_(other.alloc_)
    , key_(other.key_)
    , val_(other.val_)
{
    refAcquire();
}


Arg::Arg(Arg&& other) noexcept
    : type_(other.type_)
    , alloc_(other.alloc_)
    , key_(other.key_)
    , val_(other.val_)
{
    // It is allowed to leave the source object with an inconsistent state.
    other.alloc_ = Alloc::None;
}


Arg::~Arg() noexcept
{
    refRelease();
}


void Arg::refAcquire() noexcept
{
    if (alloc_ == Alloc::Heap) {
        if (type_ == Type::String)
            ++val_.str_->cnt_;
        else if (type_ == Type::Array)
            ++val_.arr_->cnt_;
    }
}


void Arg::refRelease() noexcept
{
    if (alloc_ == Alloc::Heap) {
        if (type_ == Type::String && --val_.str_->cnt_ == 0)
            delete val_.str_;
        else if (type_ == Type::Array && --val_.arr_->cnt_ == 0)
            delete val_.arr_;
    }
}


Arg& Arg::operator=(const Arg& other) noexcept
{
    if (this != &other) {
        refRelease();

        type_ = other.type_;
        alloc_ = other.alloc_;
        key_ = other.key_;
        val_ = other.val_;

        refAcquire();
    }

    return *this;
}


size_t Arg::count() const noexcept
{
    switch (type_) {
    case Type::Invalid:
        return 0;

    case Type::Int:
    case Type::Errno:
    case Type::Double:
    case Type::String:
        return 1;

    case Type::Array:
        return val_.arr_->siz_;
    }

    return 0;
}


size_t Arg::refCount() const noexcept
{
    switch (type_) {
    case Type::Invalid:
    case Type::Int:
    case Type::Errno:
    case Type::Double:
        return 0;

    case Type::String:
        if (alloc_ != Alloc::Heap)
            return 0;
        else
            return val_.str_->cnt_;

    case Type::Array:
        return val_.arr_->cnt_;
    }

    return 0;
}


Arg::Type Arg::type() const noexcept
{
    return type_;
}


std::string_view Arg::key() const noexcept
{
    return key_;
}


int Arg::toInt() const noexcept
{
    if (type_ != Type::Int && type_ != Type::Errno)
        return 0;

    return val_.int_;
}


double Arg::toDouble() const noexcept
{
    if (type_ != Type::Double)
        return 0;

    return val_.dbl_;
}


std::string_view Arg::toString() const noexcept
{
    if (type_ == Type::String) {
        switch (alloc_) {
        case Alloc::None:
            return val_.chr_;

        case Alloc::Stack:
            return std::string_view(val_.buf_.dat_, val_.buf_.siz_);

        case Alloc::Heap:
            return std::string_view(val_.str_->buf_, val_.str_->siz_);
        }
    } else if (type_ == Type::Errno) {
        return std::string_view(zmq_strerror(val_.int_));
    }

    return std::string_view();
}


const Arg* Arg::toArray() const noexcept
{
    if (type_ != Type::Array)
        return nullptr;

    return val_.arr_->buf_;
}


/**
 * Handler
 */

Handler::~Handler()
{
}

namespace {
inline void printkey(std::ostream& os, std::string_view key)
{
    if (!key.empty())
        os << key << ": ";
}

template<typename T>
inline void printarg(std::ostream& os, std::string_view key, T val)
{
    printkey(os, key);
    os << val;
}


inline void printargs(std::ostream& os, const Arg args[], size_t num)
{
    const char* const separator = ", ";
    const char* sep = "";

    for (size_t i = 0; i < num; ++i) {
        os << sep << args[i];
        sep = separator;
    }
}


inline void println(std::ostream& os, const Arg args[], size_t num)
{
    printargs(os, args, num);
    os << std::endl;
}
} // namespace


std::ostream& operator<<(std::ostream& os, const Arg& arg)
{
    switch (arg.type()) {
    case Arg::Invalid:
        os << "<>";
        break;
    case Arg::Type::Int:
        printarg(os, arg.key(), arg.toInt());
        break;
    case Arg::Type::Double:
        printarg(os, arg.key(), arg.toDouble());
        break;
    case Arg::Type::Errno:
    case Arg::Type::String:
        printarg(os, arg.key(), arg.toString());
        break;
    case Arg::Type::Array:
        printkey(os, arg.key());
        printargs(os, arg.toArray(), arg.count());
        break;
    }

    return os;
}


void StandardHandler::debug(const Loc&, const Arg args[], size_t num) const
{
    println(std::cout, args, num);
}

void StandardHandler::info(const Loc&, const Arg args[], size_t num) const
{
    println(std::cout, args, num);
}

void StandardHandler::warn(const Loc&, const Arg args[], size_t num) const
{
    println(std::cerr, args, num);
}

void StandardHandler::error(const Loc&, const Arg args[], size_t num) const
{
    println(std::cerr, args, num);
}

void StandardHandler::fatal(const Loc&, const Arg args[], size_t num) const
{
    println(std::cerr, args, num);
    std::abort();
}

void SilentHandler::debug(const Loc&, const Arg*, size_t) const
{
}

void SilentHandler::info(const Loc&, const Arg*, size_t) const
{
}

void SilentHandler::warn(const Loc&, const Arg*, size_t) const
{
}

void SilentHandler::error(const Loc&, const Arg*, size_t) const
{
}

void SilentHandler::fatal(const Loc&, const Arg args[], size_t num) const
{
    println(std::cerr, args, num);
    std::abort();
}


/**
 * Logger
 */

std::unique_ptr<Handler> Logger::handler_(new StandardHandler);

void Logger::installContentHandler(Handler* handler)
{
    ASSERT(handler != nullptr, "log message handler is null");

    handler_.reset(handler);
}


#define LOGGER_LEVEL(r, data, level) \
    void Logger::level(const Loc& loc, const Arg args[], size_t num) noexcept \
    { \
        try { \
            handler_->level(loc, args, num); \
        } catch (...) { \
            ASSERT(false, "Logger::" #level ", unexpected exception caught!"); \
        } \
    }
BOOST_PP_SEQ_FOR_EACH(LOGGER_LEVEL, _, (debug)(info)(warn)(error)(fatal))
#undef LOGGER_LEVEL
} // namespace log
} // namespace fuurin
