/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/zmqpart.h"
#include "fuurin/errors.h"
#include "failure.h"
#include "log.h"

#include <zmq.h>
#include <boost/config.hpp> // for BOOST_LIKELY

#include <cstring>
#include <algorithm>
#include <iomanip>


/**
 * Define ENDIANESS
 */

#if defined(FUURIN_ENDIANESS_BIG) && defined(FUURIN_ENDIANESS_LITTLE)
#error "Please #define either FUURIN_ENDIANESS_BIG or FUURIN_ENDIANESS_LITTLE, not both"
#endif

#if !defined(FUURIN_ENDIANESS_BIG) && !defined(FUURIN_ENDIANESS_LITTLE)
#define FUURIN_ENDIANESS_LITTLE
#endif

#if defined(FUURIN_ENDIANESS_BIG)
#define ENDIANESS_STRAIGHT __BIG_ENDIAN
#define ENDIANESS_OPPOSITE __LITTLE_ENDIAN
#endif

#if defined(FUURIN_ENDIANESS_LITTLE)
#define ENDIANESS_STRAIGHT __LITTLE_ENDIAN
#define ENDIANESS_OPPOSITE __BIG_ENDIAN
#endif


namespace {
inline void memcpyWithEndian(void* dest, const void* source, size_t size)
{
#if __BYTE_ORDER == ENDIANESS_STRAIGHT
    std::memcpy(dest, source, size);
#elif __BYTE_ORDER == ENDIANESS_OPPOSITE
    std::reverse_copy((uint8_t*)source, ((uint8_t*)source) + size, (uint8_t*)dest);
#else
#error "Unable to detect endianness"
#endif
}


void initMessageSize(zmq_msg_t* msg, size_t size)
{
    namespace log = fuurin::log;

    const int rc = zmq_msg_init_size(msg, size);

    if (BOOST_UNLIKELY(rc == -1)) {
        throw ERROR(ZMQPartCreateFailed, "could not create message part",
            log::Arg{
                log::Arg{"reason"sv, log::ec_t{zmq_errno()}},
                log::Arg{"size"sv, int(size)},
            });
    }
}


template<typename T>
inline void initMessageWithEndian(zmq_msg_t* msg, T val)
{
    initMessageSize(msg, sizeof(val));
    memcpyWithEndian(zmq_msg_data(msg), &val, sizeof(val));
}
} // namespace


namespace fuurin {
namespace zmq {


Part::Part()
    : msg_(*reinterpret_cast<zmq_msg_t*>(raw_.msg_))
{
    // assert raw backing array
    static_assert(sizeof(Raw) == sizeof(zmq_msg_t));
    static_assert(alignof(Raw) >= alignof(zmq_msg_t));

    const int rc = zmq_msg_init(&msg_);
    ASSERT(rc == 0, "zmq_msg_init failed");
}


Part::Part(uint8_t val)
    : msg_(*reinterpret_cast<zmq_msg_t*>(raw_.msg_))
{
    initMessageWithEndian(&msg_, val);
}


Part::Part(uint16_t val)
    : msg_(*reinterpret_cast<zmq_msg_t*>(raw_.msg_))
{
    initMessageWithEndian(&msg_, val);
}


Part::Part(uint32_t val)
    : msg_(*reinterpret_cast<zmq_msg_t*>(raw_.msg_))
{
    initMessageWithEndian(&msg_, val);
}


Part::Part(uint64_t val)
    : msg_(*reinterpret_cast<zmq_msg_t*>(raw_.msg_))
{
    initMessageWithEndian(&msg_, val);
}


Part::Part(const char* data, size_t size)
    : msg_(*reinterpret_cast<zmq_msg_t*>(raw_.msg_))
{
    initMessageSize(&msg_, size);
    std::memcpy(zmq_msg_data(&msg_), data, size);
}


Part::Part(std::string_view val)
    : Part(val.data(), val.size())
{
}


Part::Part(const std::string& val)
    : Part(val.data(), val.size())
{
}


Part::~Part() noexcept
{
    const int rc = zmq_msg_close(&msg_);
    ASSERT(rc == 0, "zmq_msg_close failed");
}


namespace {
void moveMsg(zmq_msg_t* dst, zmq_msg_t* src)
{
    const int rc = zmq_msg_move(dst, src);
    if (rc == -1)
        throw ERROR(ZMQPartMoveFailed, "could not move message part");
}
} // namespace


Part& Part::move(Part& other)
{
    moveMsg(&msg_, &other.msg_);
    return *this;
}


Part& Part::move(Part&& other)
{
    moveMsg(&msg_, &other.msg_);
    return *this;
}


Part& Part::copy(const Part& other)
{
    const int rc = zmq_msg_copy(&msg_, &other.msg_);
    if (rc == -1)
        throw ERROR(ZMQPartCopyFailed, "could not copy message part");

    return *this;
}


bool Part::operator==(const Part& other) const noexcept
{
    const size_t sz = size();
    return sz == other.size() && std::memcmp(data(), other.data(), sz) == 0;
}


bool Part::operator!=(const Part& other) const noexcept
{
    return !(*this == other);
}


const char* Part::data() const noexcept
{
    return static_cast<const char*>(zmq_msg_data(const_cast<zmq_msg_t*>(&msg_)));
}


size_t Part::size() const noexcept
{
    return zmq_msg_size(const_cast<zmq_msg_t*>(&msg_));
}


bool Part::empty() const noexcept
{
    return size() == 0u;
}


uint8_t Part::toUint8() const noexcept
{
    if (size() != 1)
        return 0;

    const char* const d = data();
    return static_cast<uint8_t>(d[0]);
}


uint16_t Part::toUint16() const noexcept
{
    if (size() != 2)
        return 0;

    const char* const d = data();
    return uint16_t(0) |
#if defined(FUURIN_ENDIANESS_LITTLE)
        (static_cast<uint16_t>(d[1]) << 8) |
        (static_cast<uint16_t>(d[0]));
#else
        (static_cast<uint16_t>(d[0]) << 8) |
        (static_cast<uint16_t>(d[1]));
#endif
}


uint32_t Part::toUint32() const noexcept
{
    if (size() != 4)
        return 0;

    const char* const d = data();
    return uint32_t(0) |
#if defined(FUURIN_ENDIANESS_LITTLE)
        (static_cast<uint32_t>(d[3]) << 24) |
        (static_cast<uint32_t>(d[2]) << 16) |
        (static_cast<uint32_t>(d[1]) << 8) |
        (static_cast<uint32_t>(d[0]));
#else
        (static_cast<uint32_t>(d[0]) << 24) |
        (static_cast<uint32_t>(d[1]) << 16) |
        (static_cast<uint32_t>(d[2]) << 8) |
        (static_cast<uint32_t>(d[3]));
#endif
}


uint64_t Part::toUint64() const noexcept
{
    if (size() != 8)
        return 0;

    const char* const d = data();
    return uint64_t(0) |
#if defined(FUURIN_ENDIANESS_LITTLE)
        (static_cast<uint64_t>(d[7]) << 56) |
        (static_cast<uint64_t>(d[6]) << 48) |
        (static_cast<uint64_t>(d[5]) << 40) |
        (static_cast<uint64_t>(d[4]) << 32) |
        (static_cast<uint64_t>(d[3]) << 24) |
        (static_cast<uint64_t>(d[2]) << 16) |
        (static_cast<uint64_t>(d[1]) << 8) |
        (static_cast<uint64_t>(d[0]));
#else
        (static_cast<uint64_t>(d[0]) << 56) |
        (static_cast<uint64_t>(d[1]) << 48) |
        (static_cast<uint64_t>(d[2]) << 40) |
        (static_cast<uint64_t>(d[3]) << 32) |
        (static_cast<uint64_t>(d[4]) << 24) |
        (static_cast<uint64_t>(d[5]) << 16) |
        (static_cast<uint64_t>(d[6]) << 8) |
        (static_cast<uint64_t>(d[7]));
#endif
}


std::string_view Part::toString() const
{
    return std::string_view(data(), size());
}


std::ostream& operator<<(std::ostream& os, const Part& msg)
{
    const std::ios_base::fmtflags f(os.flags());

    for (size_t i = 0; i < msg.size(); ++i) {
        os << std::hex << std::uppercase
           << std::setfill('0') << std::setw(2)
           << int(msg.data()[i]);
    }

    os.flags(f);

    return os;
}


} // namespace zmq
} // namespace fuurin