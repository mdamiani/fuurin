/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQMESSAGE_H
#define ZMQMESSAGE_H

#include <cstddef>
#include <cstdint>


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
 * \brief This class stores a ZMQ message to be sent or received.
 *
 * Memory for storage is allocated following the same behaviour
 * of \c zmq_msg_init* functions.
 *
 * Data is stored with the proper sending/receiving endianess.
 */
class Message final
{
    friend class Socket;

public:
    /**
     * \brief Initializes an empty message.
     *
     * This method calls \c zmq_msg_init.
     */
    explicit Message();

    /**
     * \brief Inizializes with an integer value.
     *
     * The value data is copied using the proper endianess format,
     * ready to be sent over the network.
     *
     * This method calls \c zmq_msg_init_size.
     *
     * \param[in] val Value to be stored in the message.
     *
     * \exception ZMQMessageCreateFailed The message could not be created.
     */
    ///{@
    explicit Message(uint8_t val);
    explicit Message(uint16_t val);
    explicit Message(uint32_t val);
    explicit Message(uint64_t val);
    ///{@

    /**
     * \brief Initializes with raw data.
     *
     * The message data is copied as is, without any
     * endianess convertion.
     *
     * This method calls \c zmq_msg_init_size.
     *
     * \param[in] data Data to be stored in this message.
     * \param[in] size Size of the data.
     */
    explicit Message(const char* data, size_t size);

    /**
     * \brief Releases resources for this message.
     *
     * This method calls \c zmq_msg_close.
     */
    ~Message() noexcept;

    /**
     * \brief Moves an \c other message contents to this message.
     *
     * It calls \c zmq_msg_move so the \c other message is cleared
     * and any previous contents of this message is released.
     *
     * \param other Another message to move data from.
     */
    ///{@
    void move(Message& other);
    void move(Message&& other);
    ///@}

    /**
     * \brief Checks whether an \c other message is equal to this one.
     * \param[in] other Another message to compare.
     * \return Whether messages are equal.
     */
    ///{@
    bool operator==(const Message& other) const noexcept;
    bool operator!=(const Message& other) const noexcept;
    ///@}

    /**
     * \return The internal stored data, possibly with a different endianess format.
     * \see size()
     * \see empty()
     */
    const char* data() const noexcept;

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
     * \return Converts internal data to the requested integer type.
     *         It returns 0 if the requested integer size doesn't
     *         match the internal data \ref size().
     *
     * \see data()
     * \see size()
     */
    ///{@
    uint8_t toUint8() const noexcept;
    uint16_t toUint16() const noexcept;
    uint32_t toUint32() const noexcept;
    uint64_t toUint64() const noexcept;
    ///@}

    /**
     * Disable copy.
     */
    ///{@
    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;
    ///@}


private:
    /**
     * \brief This is the backing array to hold a bare \c zmq_msg_t type.
     *
     * Size and alignment depends on values found in zmq.h header file.
     */
    struct alignas(8) Raw
    {
        char msg_[64];
    } raw_;

    /**
     * \brief ZMQ message (referencing the \ref raw_ backing storage.
     */
    zmq_msg_t& msg_;
};

} // namespace zmq
} // namespace fuurin

#endif // ZMQMESSAGE_H
