/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ARG_H
#define ARG_H


#include <string>
#include <string_view>
#include <ostream>
#include <initializer_list>
#include <atomic>


namespace fuurin {
namespace log {


/// Type which represents an error code.
struct ec_t
{
    int val;
};


/**
 * \brief Arg is a light class for variant arguments to be logged.
 *
 * It can store values of integral types with no dynamic allocations.
 *
 * When a dynamic value is passed (e.g. std::string), its content is copied and
 * managed through a simple reference counting. The dynamically allocated data
 * is shared among different copies of this argument, avoiding multiple heap
 * allocations.
 *
 * However, as a special case, when the dymanic value fits the size of the
 * biggest underlying type (~16 bytes), then it's still stored in the stack,
 * without allocations.
 *
 * When an argument is storing an array value, heap data is always allocated
 * and reference counted.
 *
 * Reference counter is manage atomically, so it's thread-safe.
 */
class Arg final
{
public:
    /**
     * \brief Type of this argument.
     */
    enum struct Type : uint8_t
    {
        Invalid, ///< Invalid argument.
        Int,     ///< Integer.
        Errno,   ///< Error Code.
        Double,  ///< Double.
        String,  ///< String.
        Array,   ///< Array of arguments.
    };


public:
    /**
     * \brief Constructs an invalid argument.
     */
    Arg() noexcept;

    /**
     * \brief Constructs a valid \c val argument, of the specified type.
     * The \ref key() of this argument will be set to \c nullptr.
     *
     * \param[in] val Value of this argument.
     *
     * \see Arg(const char*, int)
     */
    ///@{
    explicit Arg(int val) noexcept;
    explicit Arg(ec_t val) noexcept;
    explicit Arg(double val) noexcept;
    explicit Arg(std::string_view val) noexcept;
    explicit Arg(const std::string& val);
    explicit Arg(std::initializer_list<Arg> l);
    template<size_t N>
    explicit Arg(const Arg (&val)[N])
        : Arg(std::string_view(), val, N)
    {
    }
    ///@}

    /**
     * \brief Constructs a valid \c key - \c val argument, of the specified type.
     *
     * \param[in] key Key of this argument.
     * \param[in] val Value of this argument.
     *
     * \see count()
     * \see refCount()
     */
    ///@{
    explicit Arg(std::string_view key, int val) noexcept;
    explicit Arg(std::string_view key, ec_t val) noexcept;
    explicit Arg(std::string_view key, double val) noexcept;
    explicit Arg(std::string_view key, std::string_view val) noexcept;
    explicit Arg(std::string_view key, const std::string& val);
    explicit Arg(std::string_view key, std::initializer_list<Arg> l);
    template<size_t N>
    explicit Arg(std::string_view key, const Arg (&val)[N])
        : Arg(key, val, N)
    {
    }
    ///@}

    /**
     * \brief Constructs an argument by copying another one.
     * \param[in] other Another argument.
     */
    Arg(const Arg& other) noexcept;

    /**
     * \brief Constructs an argument by moving another one.
     * \param[in] other Another argument.
     */
    Arg(Arg&& other) noexcept;

    /**
     * \brief Destroys this argument.
     * \see refRelease()
     */
    ~Arg() noexcept;

    /**
     * \return The type of this argument.
     */
    Type type() const noexcept;

    /**
     * \return The key of this argument, or \c nullptr if it was not set.
     */
    std::string_view key() const noexcept;

    /**
     * \return The value of this argument if the type matches the requested value,
     *         or 0 otherwise.
     */
    ///@{
    int toInt() const noexcept;
    double toDouble() const noexcept;
    std::string_view toString() const noexcept;
    const Arg* toArray() const noexcept;
    ///@}

    /**
     * \brief Assigns another argument to this one.
     * \param[in] other Another argument.
     */
    Arg& operator=(const Arg& other) noexcept;

    /**
     * \return The number of values of this argument,
     *         that is always 1 for non-array values.
     */
    size_t count() const noexcept;

    /**
     * \return The number of arguments which are sharing the same dynamically allocated data,
     *         including this one. It returns 0 in case of no heap allocations.
     */
    size_t refCount() const noexcept;


private:
    /**
     * \brief Simple reference count to a shared payload.
     */
    template<typename T>
    struct Ref
    {
        T* const buf_;            ///< Data buffer.
        const size_t siz_;        ///< Size of buffer.
        std::atomic<size_t> cnt_; ///< Reference count.

        /**
         * \brief Initializes the reference and allocated the string buffer.
         * \param[in] size Size of the string buffer to allocate.
         */
        explicit Ref(size_t size)
            : buf_(new T[size])
            , siz_(size)
            , cnt_(0)
        {
        }

        /**
         * \brief Deletes the allocated resources.
         */
        ~Ref() noexcept
        {
            delete[] buf_;
        }
    };

    /**
     * \brief Union of supported argument values.
     */
    union Val
    {
        /// Sets the value.
        ///@{
        explicit Val(int) noexcept;
        explicit Val(double) noexcept;
        explicit Val(std::string_view) noexcept;
        explicit Val(Ref<char>*) noexcept;
        explicit Val(Ref<Arg>*) noexcept;
        ///@}

        /// Underlying type of value.
        ///@{
        int int_;
        double dbl_;
        std::string_view chr_;
        /// Underlying buffer type of value.
        struct Buf
        {
            uint8_t siz_;
            char dat_[sizeof(chr_) - sizeof(siz_)];
        } buf_;
        Ref<char>* str_;
        Ref<Arg>* arr_;
        ///@}
    };

    /**
     * \brief Type of allocation of argument value.
     */
    enum struct Alloc : uint8_t
    {
        None,  ///< No allocation / pointer to data.
        Stack, ///< Stack allocation.
        Heap,  ///< Heap allocation.
    };


private:
    /**
     * \brief Constructs a valid object by copying an array of arguments.
     * \param[in] key Key of this argument.
     * \param[in] arg Array of arguments.
     * \param[in] size Size of the array.
     */
    Arg(std::string_view key, const Arg* arg, size_t size);

    /**
     * \brief Acquires a reference counted data.
     * \see Ref
     */
    void refAcquire() noexcept;

    /**
     * \brief Releases a reference counted data.
     * \see Ref
     */
    void refRelease() noexcept;


public:
    /// Maximum size for strings stored as \ref Alloc::Stack.
    static constexpr size_t MaxStringStackSize{sizeof(Val::Buf::dat_)};


private:
    Type type_;            ///< Type of argument.
    Alloc alloc_;          ///< Type of value allocation.
    std::string_view key_; ///< Argument key.
    Val val_;              ///< Argument value.
};


/**
 * \brief Outputs the type of an argument.
 */
std::ostream& operator<<(std::ostream& os, const Arg::Type& type);


/**
 * \brief Outputs an argument.
 */
std::ostream& operator<<(std::ostream& os, const Arg& arg);


/**
 * \brief Prints an array of arguments.
 */
void printArgs(std::ostream& os, const Arg args[], size_t num);


} // namespace log
} // namespace fuurin

#endif // ARG_H
