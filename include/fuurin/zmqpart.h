/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQPART_H
#define ZMQPART_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <ostream>


#if defined(FUURIN_ENDIANESS_BIG) && defined(FUURIN_ENDIANESS_LITTLE)
#error "Please #define either FUURIN_ENDIANESS_BIG or FUURIN_ENDIANESS_LITTLE, not both"
#endif

#if !defined(FUURIN_ENDIANESS_BIG) && !defined(FUURIN_ENDIANESS_LITTLE)
#define FUURIN_ENDIANESS_LITTLE
#endif


struct zmq_msg_t;


namespace fuurin {
namespace zmq {


/**
 * \brief This class stores a ZMQ message part to be sent or received.
 *
 * Memory for storage is allocated following the same behaviour
 * of \c zmq_msg_init* functions. The semantics are the same
 * as exposed by \c zmq_msg_t (e.g. it is cleared after sending).
 *
 * Data is stored with the proper sending/receiving endianess.
 */
class Part
{
public:
    /**
     * \brief Initializes an empty part.
     *
     * This method calls \c zmq_msg_init.
     */
    explicit Part();

    /**
     * \brief Inizializes a part with value.
     *
     * The value data is copied using the proper endianess format,
     * ready to be sent over the network.
     *
     * This method calls \c zmq_msg_init_size.
     *
     * \param[in] val Value to be stored in the part.
     *
     * \exception ZMQPartCreateFailed The part could not be created.
     */
    ///@{
    explicit Part(uint8_t val);
    explicit Part(uint16_t val);
    explicit Part(uint32_t val);
    explicit Part(uint64_t val);
    explicit Part(std::string_view val);
    explicit Part(const std::string& val);
    ///@{

    /**
     * \brief Initializes with raw data.
     *
     * Data is copied as is, without any
     * endianess convertion.
     *
     * This method calls \c zmq_msg_init_size.
     *
     * \param[in] data Data to be stored in this part.
     * \param[in] size Size of the data.
     *
     * \exception ZMQPartCreateFailed The part could not be created.
     */
    explicit Part(const char* data, size_t size);

    /**
     * Type used for size initialization.
     * \see Part(msg_init_size_t, size_t)
     */
    struct msg_init_size_t
    {};
    static constexpr msg_init_size_t msg_init_size = msg_init_size_t{};

    /**
     * \brief Inizializes a part with a buffer of the specified \c size.
     *
     * This method calls \c zmq_msg_init_size.
     *
     * \param[in] size Size of this part.
     *
     * \exception ZMQPartCreateFailed The part could not be created.
     */
    explicit Part(msg_init_size_t, size_t size);

    /**
     * \brief Constructs a part by hard copying another one.
     * \param[in] other Another part.
     *
     * \exception ZMQPartCreateFailed The part could not be created.
     */
    Part(const Part& other);

    /**
     * \brief Constructs a part by moving from another one.
     * \param[in] other Another part.
     *
     * \exception ZMQPartMoveFailed The part could not be moved.
     */
    Part(Part&& other);

    /**
     * \brief Releases resources for this part.
     *
     * This method calls \c zmq_msg_close.
     *
     * \see close()
     */
    ~Part() noexcept;

    /**
     * \brief Moves an \c other part contents to this part.
     *
     * It calls \c zmq_msg_move so the \c other part is cleared
     * and any previous contents of this part is released.
     *
     * \param[in] other Another part to move data from.
     * \return A reference to this part.
     *
     * \exception ZMQPartMoveFailed The part could not be moved.
     */
    ///@{
    Part& move(Part& other);
    Part& move(Part&& other);
    ///@}

    /**
     * \return The underlying raw ZMQ pointer.
     */
    zmq_msg_t* zmqPointer() const noexcept;

    /**
     * \brief Creates a copy or share data from an \c other part.
     *
     * It calls \c zmq_msg_copy, so this messages is cleared
     * if it contains any contents.
     *
     * Modifying contents after calling this function can lead
     * to undefined behaviour.
     *
     * \param[in] other Another part to copy/share from.
     * \return A reference to this part.
     *
     * \exception ZMQPartCopyFailed The part could not be copied/shared.
     */
    Part& share(const Part& other);

    /**
     * \brief Checks whether an \c other part is equal to this one.
     * \param[in] other Another part to compare.
     * \return Whether messages are equal.
     */
    ///@{
    bool operator==(const Part& other) const noexcept;
    bool operator!=(const Part& other) const noexcept;
    ///@}

    /**
     * \brief Assigns the contents of another part by hard copying it.
     *
     * Current contents is released before assignment and data
     * is copied with memcpy.
     *
     * \param[in] other Another part.
     * \return A reference to this part.
     *
     * \exception ZMQPartCreateFailed The part could not be copied.
     *
     * \see close()
     */
    Part& operator=(const Part& other);

    /**
     * \return The internal stored data, potentially with a different endianess format.
     * \see size()
     * \see empty()
     */
    ///@{
    const char* data() const noexcept;
    char* data() noexcept;
    ///@}

    /**
     * \return The size of \ref data.
     * \see data()
     * \see empty()
     */
    size_t size() const noexcept;

    /**
     * \return Whether the \ref size() of this \ref data() is zero.
     * \see data()
     * \see size()
     */
    bool empty() const noexcept;

    /**
     * \return Whether this is a part of a multi-part message and there are more parts to receive.
     */
    bool hasMore() const;

    /**
     * \brief Sets the routing ID property to this part.
     *
     * \param[in] id The routing ID value.
     *
     * \return A reference to this part.
     * \exception ZMQPartRoutingIDFailed The routing ID could not be set.
     *
     * \see setRoutingID(uint32_t)
     * \see routingID()
     */
    Part& withRoutingID(uint32_t id);

    /**
     * \brief Sets the routing ID property to this part.
     *
     * This method calls \c zmq_msg_set_routing_id.
     *
     * \param[in] id The routing ID value.
     * \exception ZMQPartRoutingIDFailed The routing ID could not be set.
     *
     * \see withRoutingID(uint32_t)
     * \see routingID()
     */
    void setRoutingID(uint32_t id);

    /**
     * \brief Returns the routing ID of this part.
     *
     * This method calls \c zmq_msg_routing_id.
     *
     * \return The routing ID or 0 if it was not set.
     *
     * \see withRoutingID(uint32_t)
     * \see setRoutingID(uint32_t)
     */
    uint32_t routingID() const noexcept;

    /**
     * \brief Sets the group property to this part.
     *
     * \param[in] group A null terminated string, no more than \c ZMQ_GROUP_MAX_LENGTH.
     *
     * \return A reference to this part.
     * \exception ZMQPartGroupFailed The group could not be set.
     *
     * \see setGroup(const char *)
     * \see group()
     */
    Part& withGroup(const char* group);

    /**
     * \brief Sets the group property to this part.
     *
     * This method calls \c zmq_msg_set_group.
     *
     * \param[in] group A null terminated string, no more than \c ZMQ_GROUP_MAX_LENGTH.
     * \exception ZMQPartGroupFailed The group could not be set.
     *
     * \see withGroup(const char*)
     * \see group()
     */
    void setGroup(const char* group);

    /**
     * \brief Returns the group of this part.
     *
     * This method calls \c zmq_msg_group.
     *
     * \return A null terminated string, no more than \c ZMQ_GROUP_MAX_LENGTH.
     *
     * \see withGroup(const char*)
     * \see setGroup(const char*)
     */
    const char* group() const noexcept;

    /**
     * \return Converts internal data to the requested integer type.
     *         It returns 0 if the requested integer size doesn't
     *         match the internal data \ref size().
     *
     * \see data()
     * \see size()
     */
    ///@{
    uint8_t toUint8() const noexcept;
    uint16_t toUint16() const noexcept;
    uint32_t toUint32() const noexcept;
    uint64_t toUint64() const noexcept;
    ///@}

    /**
     * \return A string view upon the internal data, without any endianess conversions.
     * \see data()
     * \see size()
     */
    std::string_view toString() const;


protected:
    /**
     * \brief Copies an integral type value to a destination buffer, with endianess conversion.
     * \param[in] dest Destination buffer.
     * \param[in] value Value to copy.
     * \see void memcpyWithEndian(void*, const void*, size_t);
     */
    template<typename T>
    static std::enable_if_t<std::is_integral_v<std::decay_t<T>> && std::is_unsigned_v<std::decay_t<T>>, void>
    memcpyWithEndian(void* dest, T&& value)
    {
        memcpyWithEndian(dest, &value, sizeof(T));
    }

    /**
     * \brief Extracts an integral type value from a source buffer, with endianess conversion.
     * \param[in] source Source buffer.
     * \return The extracted value.
     * \see void memcpyWithEndian(void*, const void*, size_t);
     */
    template<typename T>
    static std::enable_if_t<std::is_integral_v<std::decay_t<T>> && std::is_unsigned_v<std::decay_t<T>>, T>
    memcpyWithEndian(const void* source)
    {
        T value;
        memcpyWithEndian(&value, source, sizeof(T));
        return value;
    }
    ///@}

    /**
     * \brief Copies memory to/from internal buffer, with proper endianess.
     *
     * If the native endianess matches the sending one, then data is copied
     * as is without any conversion.
     *
     * \param[in] dest Destination buffer.
     * \param[in] source Source buffer.
     * \param[in] size Size of buffer.
     *
     * \see buffer()
     */
    static void memcpyWithEndian(void* dest, const void* source, size_t size);


private:
    /**
     * \brief Releases resources for this part.
     *
     * This method calls \c zmq_msg_close.
     */
    void close() noexcept;


private:
    /**
     * \brief This is the backing array to hold a bare \c zmq_msg_t type.
     * Size and alignment depends on values found in zmq.h header file.
     */
    struct alignas(void*) Raw
    {
        char msg_[64];
    } raw_;

    /**
     * \brief ZMQ part (referencing the \ref raw_ backing storage.
     */
    zmq_msg_t& msg_;
};


/**
 * \brief Outputs a part.
 * \param[in] os Output stream.
 * \param[in] msg Part to print.
 */
std::ostream& operator<<(std::ostream& os, const Part& msg);


} // namespace zmq
} // namespace fuurin

#endif // ZMQPART_H
