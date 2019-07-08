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
 * of \c zmq_msg_init* functions.
 *
 * Data is stored with the proper sending/receiving endianess.
 */
class Part final
{
    friend class Socket;

public:
    /**
     * \brief Initializes an empty part.
     *
     * This method calls \c zmq_msg_init.
     */
    explicit Part();

    /**
     * \brief Inizializes with an integer value.
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
    ///{@
    explicit Part(uint8_t val);
    explicit Part(uint16_t val);
    explicit Part(uint32_t val);
    explicit Part(uint64_t val);
    explicit Part(std::string_view val);
    explicit Part(const std::string& val);
    ///{@

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
    ///{@
    Part& move(Part& other);
    Part& move(Part&& other);
    ///@}

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
    ///{@
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
     * \return A string view upon the internal data, without any endianess conversions.
     * \see data()
     * \see size()
     */
    std::string_view toString() const;


private:
    /**
     * \brief Releases resources for this part.
     *
     * This method calls \c zmq_msg_close.
     */
    inline void close() noexcept;


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
 * \param[in] msg Part to print.
 */
std::ostream& operator<<(std::ostream& os, const Part& msg);


} // namespace zmq
} // namespace fuurin

#endif // ZMQPART_H
