/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_UUID_H
#define FUURIN_UUID_H

#include <array>
#include <functional>
#include <string_view>
#include <ostream>
#include <mutex>


namespace fuurin {

namespace zmq {
class Part;
} // namespace zmq


/**
 * \brief Provides an universally unique identifier (UUID) data type,
 * with some convenient methods.
 */
class Uuid
{
public:
    /// UUID data type;
    using Bytes = std::array<uint8_t, 16>;

    /// UUID string representation data type;
    using Srepr = std::array<char, Bytes{}.size() * 2 + 4>;

    /// UUID string representation for null value.
    static constexpr std::string_view NullFmt = "00000000-0000-0000-0000-000000000000";

    /// Namespace values.
    struct Ns
    {
        /// Namespace for fully-qualified domain names.
        static const Uuid Dns;

        /// Namespace for URLs.
        static const Uuid Url;

        /// Namespace for ISO Object IDs.
        static const Uuid Oid;

        /// Namespace for X.500 Distinguished Names.
        static const Uuid X500dn;
    };

    /**
     * \brief Initializes a null UUID.
     *
     * \see isNull()
     */
    Uuid();

    /**
     * \brief Copy constructor.
     */
    Uuid(const Uuid& other);

    /**
     * \return Size in bytes of the uuid (always 16).
     *
     * \see Bytes
     */
    constexpr size_t size() const noexcept
    {
        return bytes_.size();
    }

    /**
     * \brief Check whether this UUID equals to \ref NullFmt.
     */
    bool isNull() const noexcept;

    /**
     * \return UUID bytes.
     */
    const Bytes& bytes() const noexcept;

    /**
     * \brief Returns the full string representation of this UUID.
     *
     * The string representation is created at construction time,
     * so the internal data bytes must not manually changed.
     *
     * \return A string view on internal string data.
     *
     * \see toShortString()
     */
    std::string_view toString() const;

    /**
     * \brief Returns the short string representation of this UUID.
     *
     * \return A string view on internal string data.
     *
     * \see toString()
     */
    std::string_view toShortString() const;

    /**
     * \brief Assignment operator.
     * \param[in] rhs Uuid to copy from.
     */
    Uuid& operator=(const Uuid& rhs);

    /**
     * \brief Comparing operator.
     * \param[in] rhs Uuid to compare with.
     * \return \c true if both UUID are the same.
     */
    ///@{
    bool operator==(const Uuid& rhs) const;
    bool operator!=(const Uuid& rhs) const;
    ///@}


public:
    /**
     * \brief Converts a string representation to an UUID.
     *
     * The string must be of the form:
     * "hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh",
     * where each "h" has to match the regular
     * expression "[0-9a-fA-F]".
     *
     * \param[in] str String representation.
     *
     * \return A new UUID object.
     *
     * \exception std::runtime_error When input is invalid.
     */
    ///@{
    static Uuid fromString(std::string_view str);
    static Uuid fromString(const std::string& str);
    ///@}

    /**
     * \brief Create an UUID object from bytes.
     *
     * \param[in] b Raw bytes.
     *
     * \return A new UUID object.
     */
    static Uuid fromBytes(const Bytes& b);

    /**
     * \brief Creates a random V4 UUID.
     *
     * \return A new UUID object.
     *
     * \exception std::runtime_error When could not get entropy from the operating system.
     */
    static Uuid createRandomUuid();

    /**
     * \brief Creates a namespace V5 UUID.
     *
     * \param[in] ns Namespace UUID.
     * \param[in] name Name string.
     *
     * \return A new UUID object.
     *
     * \exception std::runtime_error When either namespace or name are invalid.
     */
    ///@{
    static Uuid createNamespaceUuid(const Uuid& ns, std::string_view name);
    static Uuid createNamespaceUuid(const Uuid& ns, const std::string& name);
    ///@}


public:
    /**
     * \brief Creates new \ref Uuid from a \ref zmq::PartMulti packed \ref zmq::Part.
     *
     * \param[in] part Packed uuid.
     *
     * \return A new uuid instance.
     *
     * \see zmq::PartMulti::unpack(const Part&)
     */
    static Uuid fromPart(const zmq::Part& part);

    /**
     * \brief Converts this \ref Uuid to a \ref zmq::PartMulti packed \ref zmq::Part.
     *
     * \return A packed \ref zmq::Part representing this \ref Uuid.
     *
     * \see zmq::PartMulti::pack(Args&&...)
     */
    zmq::Part toPart() const;


private:
    /**
     * \brief Updates the string representation.
     */
    void cacheSrepr() const;


private:
    Bytes bytes_; ///< UUID bytes.

    mutable std::mutex reprMux_; ///< UUID string conversion mutex.
    mutable Srepr srepr_;        ///< UUID string representation.
    mutable bool cached_;        ///< UUID string cached.
};


///< Streams to printable form a \ref SyncMachine::State value.
std::ostream& operator<<(std::ostream& os, const Uuid& v);

} // namespace fuurin


namespace std {

/**
 * \brief Makes \ref fuurin::Uuid hashable.
 */
template<>
struct hash<fuurin::Uuid>
{
    /// Hashing operator.
    size_t operator()(const fuurin::Uuid& n) const;
};

} // namespace std

#endif // FUURIN_UUID_H
