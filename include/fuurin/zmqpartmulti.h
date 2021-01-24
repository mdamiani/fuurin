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
#include <array>
#include <functional>
#include <iterator>


namespace fuurin {
namespace zmq {


/**
 * \brief Type safe multi part buffer.
 *
 * This class stores multiple message parts into a single \ref Part buffer,
 * which can be sent as it were just one single part to a \ref Socket.
 *
 * Allowed arguments are the same as the ones supported by any \ref Part,
 * plus char arrays and a \ref Part object itself. Integral types are potentially
 * endianess converted, instead string, char array or \ref Part arguments are
 * copied without any convertion.
 *
 * For every string or \ref Part argument, an additional 4 bytes header is
 * added to the buffer, in order to store the amount of subsequent bytes.
 * Conversely, the size of any integral type or char arrays is always well known.
 *
 * Objects of the same type can be packed into an iterable \ref Part, that is
 * data with a variable number of items. Item's data is packed one by one,
 * after a header which is made up of 4 bytes for the total length and
 * other 4 bytes for actual number of items.
 *
 * This class cannot be instantiated, it is just a helper namespace to
 * manipulate \ref Part objects.
 */
class PartMulti final : private Part
{
private:
    /**
     * TYPE TRAITS.
     */

    /**
     * \brief Static check whether the type is integral and unsigned.
     */
    template<typename T, typename = void>
    struct isIntegralType : std::false_type
    {};
    template<typename T>
    struct isIntegralType<T,
        typename std::enable_if_t<                  //
            std::is_integral_v<std::decay_t<T>> &&  //
                !std::is_signed_v<std::decay_t<T>>, //
            void>> : std::true_type
    {
    };


    /**
     * \brief Whether the type is a string or a \ref Part.
     */
    template<typename T, typename = void>
    struct isStringType : std::false_type
    {};
    template<typename T>
    struct isStringType<T,
        typename std::enable_if_t<                               //
            std::is_same_v<std::decay_t<T>, std::string_view> || //
                std::is_same_v<std::decay_t<T>, std::string> ||  //
                std::is_same_v<std::decay_t<T>, Part>,
            void>> : std::true_type
    {
    };


    /**
     * \brief Static check whether the type is char array.
     */
    template<typename T>
    struct isCharArrayType : std::false_type
    {};
    /// Type trait utility to match a signed or unsigned char array.
    template<typename I>
    struct mustCharArrayItem
        : std::enable_if<
              std::is_same_v<I, char> ||
                  std::is_same_v<I, unsigned char>,
              std::size_t>
    {};
    template<typename I, typename mustCharArrayItem<I>::type N>
    struct isCharArrayType<std::array<I, N> const&> : std::true_type
    {};
    template<typename I, typename mustCharArrayItem<I>::type N>
    struct isCharArrayType<std::array<I, N>&> : std::true_type
    {};
    template<typename I, typename mustCharArrayItem<I>::type N>
    struct isCharArrayType<std::array<I, N> const> : std::true_type
    {};
    template<typename I, typename mustCharArrayItem<I>::type N>
    struct isCharArrayType<std::array<I, N>> : std::true_type
    {};
    template<typename I, typename mustCharArrayItem<I>::type N>
    struct isCharArrayType<std::array<I, N>&&> : std::true_type
    {};


    /**
     * \brief Static check whether the type is an iterator type.
     */
    template<typename T, typename = void>
    struct isIteratorType : std::false_type
    {};
    template<typename T>
    struct isIteratorType<T,
        typename std::enable_if_t<!std::is_same_v<             //
            typename std::iterator_traits<T>::value_type, void //
            >>> : std::true_type                               //
    {
        using iterator_type = typename std::iterator_traits<T>::value_type;
    };
    template<typename T>
    struct isIteratorType<std::back_insert_iterator<T>> : std::true_type
    {
        using iterator_type = typename T::value_type;
    };
    template<typename T>
    struct isIteratorType<std::front_insert_iterator<T>> : std::true_type
    {
        using iterator_type = typename T::value_type;
    };
    template<typename T>
    struct isIteratorType<std::insert_iterator<T>> : std::true_type
    {
        using iterator_type = typename T::value_type;
    };


    /// Helper type for assertions.
    template<class T>
    struct dependent_false : std::false_type
    {};


    /**
     * \brief Causes a compilation error due to unsupported type.
     */
    template<typename T>
    static constexpr void assertFalseType()
    {
        using type = std::decay_t<T>;
        static_assert(dependent_false<type>::value, "type not supported");
    }


public:
    /**
     * PUBLIC INTERFACE.
     */

    /// Type to store the length of a string part.
    using string_length_t = uint32_t;
    /// Type to store the length of an iterable part.
    using iterable_length_t = uint32_t;


    /**
     * \brief Default constructor is disabled.
     */
    PartMulti() noexcept = delete;


    /**
     * \brief Creates and packs a multi part message, by fixed arguments.
     *
     * It stores a fixed number of items, both integral types and string objects
     * which size does not exceeds exceeds 4 GiB.
     *
     * \param[in] args Variable number of arguments to store.
     *
     * \return A \ref Part with packed parameters.
     *
     * \exception ZMQPartCreateFailed The multi part could not be created.
     *
     * \see unpack(const Part& pm)
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
     * \brief Extracts a multi parts message to a tuple, by fixed arguments.
     *
     * It extracts the fixed amount of requested types, from the passed \ref Part.
     *
     * \param[in] pm Part or string to unpack from.
     *
     * \return A tuple of the requested types.
     *
     * \exception ZMQPartAccessFailed The multi part buffer could not be extracted,
     *                                with the requested types.
     *
     * \see pack(Args&&...)
     * \see unpack2(const Part&, size_t)
     * \see unpack1(const Part&, size_t)
     * \see isStringType
     */
    template<typename... Args, typename P>
    static std::enable_if_t<isStringType<P>::value, std::tuple<Args...>>
    unpack(P&& pm)
    {
        constexpr auto sz = sizeof...(Args);

        if constexpr (sz == 0)
            return std::tuple<>();
        else if constexpr (sz == 1)
            return unpack1<Args...>(pm, 0);
        else
            return unpack2<Args...>(pm, 0);
    }


    /**
     * \brief Creates and pack a multi part message, iterating over a container.
     *
     * Packs data to a \ref zmq::Part which will contain a variable number of items,
     * to be subsequently accessed with the corresponding iterable version of
     * unpack method.
     *
     * \param[in] first The beginning of the input iterator to the first element.
     * \param[in] last Input iterator to the end.
     *
     * \return A \ref Part with packed parameters.
     *
     * \exception ZMQPartCreateFailed The multi part could not be created.
     *
     * \see unpack(const Part&, OutputIt)
     * \see pack(Args&&...)
     * \see pack2(Part&, size_t, T&&)
     */
    ///@{
    template<typename T, typename InputIt>
    static std::enable_if_t<isIteratorType<InputIt>::value, Part>
    pack(InputIt first, InputIt last)
    {
        static_assert(sizeof(string_length_t) < sizeof(uint64_t),
            "string_length_t must be less than uint64_t");
        static_assert(sizeof(iterable_length_t) < sizeof(uint64_t),
            "iterable_length_t must be less than uint64_t");

        string_length_t size = sizeof(string_length_t) + sizeof(iterable_length_t);
        iterable_length_t count = 0;

        for (auto item = first; item != last; ++item) {
            const size_t sz = tsize(T(*item));
            if (uint64_t(size) + sz < size ||
                uint64_t(size) + sz > std::numeric_limits<string_length_t>::max()) {
                throwCreateError("size exceeds uint32_t max");
            }
            if (uint64_t(count) + 1 > std::numeric_limits<iterable_length_t>::max()) {
                throwCreateError("number of elements exceeds uint32_t max");
            }
            size += sz;
            ++count;
        }

        Part pm{Part::msg_init_size, size};
        size_t pos = pack2(pm, 0, size);
        pos += pack2(pm, pos, count);

        for (auto item = first; item != last; ++item) {
            pos += pack2(pm, pos, T(*item));
        }

        return pm;
    }

    template<typename InputIt>
    static std::enable_if_t<isIteratorType<InputIt>::value, Part>
    pack(InputIt first, InputIt last)
    {
        using TT = typename isIteratorType<InputIt>::iterator_type;
        return pack<TT>(first, last);
    }
    ///@}


    /**
     * \brief Extract a multi parts message to a tuple, from an iterable data.
     *
     * See \ref unpack(P&&, OutputIt) for description.
     *
     * \param[in] pm Part or string to unpack from.
     * \param[out] visit Visit function for every item.
     *
     * \exception ZMQPartAccessFailed The multi part buffer could not be extracted.
     *
     * \see pack(InputIt, InputIt)
     * \see unpack(const Part&, OutputIt)
     * \see unpack2(const Part&, size_t)
     * \see unpack1(const Part&, size_t)
     * \see isStringType
     */
    template<typename T, typename Visitor, typename P>
    static std::enable_if_t<std::is_invocable<Visitor, T>::value && isStringType<P>::value, void>
    unpack(P&& pm, Visitor visit)
    {
        auto [size, count] = unpack2<string_length_t, iterable_length_t>(pm, 0);
        size_t pos = sizeof(size) + sizeof(count);

        for (iterable_length_t i = 0; i < count; ++i) {
            auto [item] = unpack1<T>(pm, pos);
            pos += tsize(item);
            visit(std::move(item));
        }
    }


    /**
     * \brief Extract a multi parts message to a tuple, from an iterable data.
     *
     * Unpacks data in a \ref Part, which contains a variable number of items,
     * packed with the corresponding iterable version of pack method.
     *
     * \param[in] pm Part or string to unpack from.
     * \param[out] d_first The beginning of the destination range.
     *
     * \exception ZMQPartAccessFailed The multi part buffer could not be extracted.
     *
     * \see pack(InputIt, InputIt)
     * \see unpack(const Part&, Visitor)
     * \see unpack2(const Part&, size_t)
     * \see unpack1(const Part&, size_t)
     * \see isStringType
     */
    ///@{
    template<typename T, typename OutputIt, typename P>
    static std::enable_if_t<isIteratorType<OutputIt>::value && isStringType<P>::value, void>
    unpack(P&& pm, OutputIt d_first)
    {
        using TT = typename isIteratorType<OutputIt>::iterator_type;
        unpack<T>(pm, [&d_first](T&& v) {
            *d_first++ = TT(v);
        });
    }

    template<typename OutputIt, typename P>
    static std::enable_if_t<isIteratorType<OutputIt>::value && isStringType<P>::value, void>
    unpack(P&& pm, OutputIt d_first)
    {
        using TT = typename isIteratorType<OutputIt>::iterator_type;
        unpack<TT>(pm, d_first);
    }
    ///@}


private:
    /**
     * IMPLEMENTATION.
     */

    ///< Test class.
    friend class TestPartMulti;


    /**
     * \brief Packs arguments into the part's internal buffer.
     *
     * \param[out] pm Part to pack to.
     * \param[in] pos Starting position in the internal buffer.
     * \param[in] part Part to pack at the passed position.
     * \param[in] args Other arguments to pack.
     *
     * \return Size of the last packed argument.
     *
     * \exception ZMQPartCreateFailed Either \c position exceeds the buffer size or
     *     \c part size exceeds 4 GiB.
     */
    ///@{
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

        } else if constexpr (isCharArrayType<T>()) {
            if (part.size() > 0) {
                std::copy_n(part.data(), sz, buf);
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
     * \param[in] pm Part to unpack from.
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
    template<typename T, typename... Args, typename P>
    static std::tuple<T, Args...> unpack2(P&& pm, size_t pos)
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
     * \param[in] pm Part to unpack from.
     * \param[in] pos Position within the internal buffer, where to unpack the tuple.
     *
     * \return The tuple with the object of the requested type,
     *      extracted from the internal buffer.
     *
     * \see unpack2
     * \see psize(const Part&, size_t)
     */
    template<typename T, typename P>
    static std::tuple<std::remove_cv_t<std::remove_reference_t<T>>>
    unpack1(P&& pm, size_t pos)
    {
        using TT = std::remove_cv_t<std::remove_reference_t<T>>;
        const char* const buf = &pm.data()[pos];
        const auto len = psize<T>(pm, pos);

        if constexpr (isIntegralType<T>()) {
            (void)(len); // fix unused variable compiler warning
            return {Part::memcpyWithEndian<TT>(buf)};

        } else if constexpr (isStringType<T>()) {
            return {TT{buf + sizeof(string_length_t), len - sizeof(string_length_t)}};

        } else if constexpr (isCharArrayType<T>()) {
            TT arr;
            std::copy_n(buf, len, arr.begin());
            return {std::move(arr)};

        } else {
            assertFalseType<T>();
        }
    }


    /**
     * \brief Calculates the size of any supported packed type.
     *
     * The size of any integral type is calculated with \c sizeof.
     * Instead the size of a string argument is its \c size() itself,
     * plus 4 bytes.
     *
     * \param[in] pm Part which contains the packed type.
     * \param[in] pos Offset of the packed type in part.
     *
     * \return Total size in bytes of the packed type.
     *
     * \exception ZMQPartAccessFailed The integral type,
     *      or the size of the string type, exceeds the length of the internal buffer,
     *      from the specified starting position.
     */
    template<typename T, typename P>
    static size_t psize(P&& pm, size_t pos)
    {
        const char* const buf = &pm.data()[pos];

        if constexpr (isIntegralType<T>()) {
            if (accessOutOfBoundary(pm, pos, sizeof(T)))
                throwAccessError("could not extract integral type");

            return sizeof(T);

        } else if constexpr (isStringType<T>()) {
            if (accessOutOfBoundary(pm, pos, sizeof(string_length_t)))
                throwAccessError("could not extract length of string type");

            const string_length_t len = Part::memcpyWithEndian<string_length_t>(buf);
            pos += sizeof(string_length_t);

            if (accessOutOfBoundary(pm, pos, len))
                throwAccessError("could not extract contents of string type");

            return sizeof(string_length_t) + len;

        } else if constexpr (isCharArrayType<T>()) {
            const size_t len = std::tuple_size<T>::value;

            if (accessOutOfBoundary(pm, pos, len))
                throwAccessError("could not extract contents of array type");

            return len;

        } else {
            assertFalseType<T>();
        }

        return 0;
    }


    /**
     * \brief Calculates the size of any supported type to pack.
     *
     * The size of any integral type is calculated with \c sizeof.
     * Instead the size of a string argument is its \c size() itself,
     * plus 4 bytes.
     *
     * \param[in] s Argument to query for size.
     *
     * \return Total size in bytes of the type to pack.
     */
    template<typename T>
    static size_t tsize(T&& s)
    {
        if constexpr (isIntegralType<T>()) {
            return sizeof(s);
        } else if constexpr (isStringType<T>()) {
            return sizeof(string_length_t) + s.size();
        } else if constexpr (isCharArrayType<T>()) {
            return s.size();
        } else {
            assertFalseType<T>();
        }
        return 0;
    }


    /**
     * \brief Checks whether there is an out of bound access.
     * \param[in] pm Part to check.
     * \param[in] pos Starting position whithin the internal buffer.
     * \param[in] sz Size to access starting from \c pos.
     * \return True in case the total requested length exceeds buffer \ref size().
     */
    template<typename P>
    static inline bool accessOutOfBoundary(P&& pm, size_t pos, size_t sz)
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
};

} // namespace zmq
} // namespace fuurin

#endif // ZMQPARTMULTI_H
