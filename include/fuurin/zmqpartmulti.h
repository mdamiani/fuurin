#ifndef ZMQPARTMULTI_H
#define ZMQPARTMULTI_H

#include "fuurin/zmqpart.h"
#include "fuurin/errors.h"

#include <type_traits>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <utility>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <functional>


namespace fuurin {
namespace zmq {


/**
 * \brief Type safe multi part buffer.
 *
 * This class stores multiple message parts into a single \ref Part buffer,
 * which can be sent as it were just one single part to a \ref Socket.
 *
 * Allowed arguments are the same as the ones supported by any \ref Part,
 * plus a \ref Part object itself. Integral types are potentially endianess
 * converted, instead string or \ref Part arguments are copied without any
 * convertion.
 *
 * For every string or \ref Part argument, an additional 4 bytes header is
 * added to the buffer, in order to store the amount of subsequent bytes.
 * Instead the size of any integral type is always well known.
 *
 * This class cannot be instantiated, it is just a helper namespace to
 * manipulate \ref Part objects.
 */
class PartMulti final : private Part
{
public:
    /// Type to store the length of a string part.
    using string_length_t = uint32_t;


    /**
     * \brief Default constructor is disabled.
     */
    PartMulti() noexcept = delete;


    /**
     * \brief Creates and packs a multi part message.
     *
     * It can store integral type and string objects which size does not
     * exceeds exceeds 4 GiB.
     *
     * \param[in] args Variable number of arguments to store.
     *
     * \exception ZMQPartCreateFailed The multi part could not be created.
     *
     * \see pack2(size_t, T&&, Args&&...)
     */
    template<typename... Args>
    static Part pack(Args&&... args)
    {
        Part pm{Part::msg_init_size, (tsize(std::forward<Args>(args)) + ... + 0)};
        pack2(pm, 0, args...);
        return pm;
    }


    /**
     * \brief Extract a multi parts message to a tuple.
     *
     * \retrun A tuple of the requested types.
     *
     * \exception ZMQPartAccessFailed The multi part buffer could not be extracted,
     *      with the requested types.
     *
     * \see unpack2(size_t)
     * \see unpack1(size_t)
     */
    template<typename... Args>
    static std::tuple<Args...> unpack(const Part& pm)
    {
        constexpr auto sz = sizeof...(Args);

        if constexpr (sz == 0)
            return std::tuple<>();
        else if constexpr (sz == 1)
            return unpack1<Args...>(pm, 0);
        else
            return unpack2<Args...>(pm, 0);
    }


private:
    friend class TestPartMulti;


    /**
     * \brief Packs arguments into the part's internal buffer.
     *
     * \param[in] pos Starting position in the internal buffer.
     * \param[in] part Part to pack at the passed position.
     * \param[in] args Other arguments to pack.
     *
     * \exception ZMQPartCreateFailed Either \c position exceeds the buffer size or
     *     \c part size exceeds 4 GiB.
     */
    ///{@
    template<typename T, typename... Args>
    static size_t pack2(Part& pm, size_t pos, T&& part, Args&&... args)
    {
        return pack2(pm, pos + pack2(pm, pos, std::forward<T>(part)), std::forward<Args>(args)...);
    }


    template<typename T>
    static size_t pack2(Part& pm, size_t pos, T&& part)
    {
        const size_t sz = tsize(std::forward<T>(part));
        char* const buf = &pm.data()[pos];

        if (accessOutOfBoundary(pm, pos, sz))
            throwCreateError("access out of bounds");

        if constexpr (isIntegralType<T>()) {
            Part::memcpyWithEndian(buf, &part, sz);

        } else if constexpr (isStringType<T>()) {
            if (part.size() > std::numeric_limits<string_length_t>::max())
                throwCreateError("size exceeds uint32_t max");

            Part::memcpyWithEndian<string_length_t>(buf, part.size());

            if (part.size() > 0) {
                std::memcpy(buf + sizeof(string_length_t), part.data(), sz - sizeof(string_length_t));
            }
        } else {
            assertFalseType<T>();
        }

        return sz;
    }


    static size_t pack2(Part&, size_t)
    {
        return 0;
    }
    ///@}

    /**
     * \brief Unpacks a tuple of multiple types, from the internal buffer.
     *
     * \param[in] pos Position within the internal buffer, where to unpack the tuple.
     *
     * \return The tuple with objects of the requested types,
     *      extracted from the internal buffer.
     *
     * \exception ZMQPartAccessFailed At least one of the integral types,
     *      or the size of a string type, exceeds the length of the internal buffer,
     *      from the specified starting position.
     *
     * \see unpack1
     */
    template<typename T, typename... Args>
    static std::tuple<T, Args...> unpack2(const Part& pm, size_t pos)
    {
        auto tuple = unpack1<T>(pm, pos);
        const size_t sz = tsize(std::forward<T>(std::get<0>(tuple)));

        if constexpr (sizeof...(Args) > 1)
            return std::tuple_cat(std::move(tuple), unpack2<Args...>(pm, pos + sz));
        else
            return std::tuple_cat(std::move(tuple), unpack1<Args...>(pm, pos + sz));
    }


    /**
     * \brief Unpacks a tuple of a single type, from the internal buffer.
     *
     * \param[in] pos Position within the internal buffer, where to unpack the tuple.
     * \return The tuple with the object of the requested type,
     *      extracted from the internal buffer.
     * \exception ZMQPartAccessFailed The the integral type,
     *      or the size of the string type, exceeds the length of the internal buffer,
     *      from the specified starting position.
     * \see unpack2
     */
    template<typename T>
    static std::tuple<T> unpack1(const Part& pm, size_t pos)
    {
        const char* const buf = &pm.data()[pos];

        if constexpr (isIntegralType<T>()) {
            if (accessOutOfBoundary(pm, pos, sizeof(T)))
                throwAccessError("could not extract integral type");

            return std::tuple(Part::memcpyWithEndian<T>(buf));

        } else if constexpr (isStringType<T>()) {
            if (accessOutOfBoundary(pm, pos, sizeof(string_length_t)))
                throwAccessError("could not extract length of string type");

            const string_length_t len = Part::memcpyWithEndian<string_length_t>(buf);
            pos += sizeof(string_length_t);

            if (accessOutOfBoundary(pm, pos, len))
                throwAccessError("could not extract contents of string type");

            return std::tuple(T{buf + sizeof(string_length_t), len});

        } else {
            assertFalseType<T>();
        }
    }


    /**
     * \brief Calculates the size of any supported type.
     *
     * The size of any integral type is calculated with \c sizeof.
     * Instead the size of a string argument is its \c size() itself,
     * plus 4 bytes.
     *
     * \param[in]a s Argument to query for size.
     */
    template<typename T>
    static size_t tsize(T&& s)
    {
        if constexpr (isIntegralType<T>()) {
            return sizeof(s);
        } else if constexpr (isStringType<T>()) {
            return sizeof(string_length_t) + s.size();
        } else {
            assertFalseType<T>();
        }
    }


    /**
     * \return Whether the type is integral and unsigned.
     */
    template<typename T>
    static constexpr bool isIntegralType()
    {
        using type = std::decay_t<T>;
        return std::is_integral_v<type> && !std::is_signed_v<type>;
    }


    /**
     * \return Whether the type is a string or a \ref Part.
     */
    template<typename T>
    static constexpr bool isStringType()
    {
        using type = std::decay_t<T>;
        return (false ||
            std::is_same_v<type, std::string_view> ||
            std::is_same_v<type, std::string> ||
            std::is_same_v<type, Part>);
    }


    /**
     * \brief Causes a compilation error due to unsupported type.
     */
    template<typename T>
    static constexpr void assertFalseType()
    {
        using type = std::decay_t<T>;
        static_assert(dependent_false<type>::value, "type not supported");
    }


    /**
     * \brief Checks whether there is an out of bound access.
     * \param[in] pos Starting position whithin the internal buffer.
     * \param[in] sz Size to access starting from \c pos.
     * \return True in case the total requested length exceeds buffer \ref size().
     */
    static inline bool accessOutOfBoundary(const Part& pm, size_t pos, size_t sz)
    {
        return !(pos <= pm.size() && pos + sz >= pos && pos + sz <= pm.size());
    }


    /**
     * \brief Helper function to throw an error at creation time.
     */
    static void throwCreateError(std::string_view reason)
    {
        throw ERROR(ZMQPartCreateFailed, "could not create multi part",
            log::Arg{std::string_view("reason"), reason});
    }


    /**
     * \brief Helper function to throw an error at access time.
     */
    static void throwAccessError(std::string_view reason)
    {
        throw ERROR(ZMQPartAccessFailed, "could not access multi part",
            log::Arg{std::string_view("reason"), reason});
    }


    /// Helper type for assertions.
    template<class T>
    struct dependent_false : std::false_type
    {};
};

} // namespace zmq
} // namespace fuurin

#endif // ZMQPARTMULTI_H
