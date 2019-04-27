/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include <string>
#include <ostream>
#include <initializer_list>


namespace fuurin {
namespace log {

/**
 * \brief Location of the log message.
 */
struct Loc
{
    const char* const file;  ///< File name where the log happened.
    const unsigned int line; ///< Code line where the log happened.
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
 * However, as a special case, when the dymanic value fits the size of the biggest
 * value (8 bytes), then it's still stored in the stack, without allocations.
 *
 * When an argument is storing an array value, a heap data is always allocated
 * and reference counted.
 */
class Arg
{
public:
    /**
     * \brief Type of this argument.
     */
    enum Type : uint8_t
    {
        Invalid, ///< Invalid argument.
        Int,     ///< Integer.
        Double,  ///< Double.
        CString, ///< Null terminated C string.
        Array,   ///< Array of arguments.
    };


public:
    /**
     * \brief Constructs an invalid argument.
     */
    Arg() noexcept;

    /**
     * \brief Destroys this argument.
     */
    ~Arg() noexcept;

    /**
     * \brief Constructs a valid \c val argument, of the specified type.
     * The \ref key() of this argument will be set to \c nullptr.
     *
     * \param[in] val Value of this argument.
     *
     * \see Arg(const char*, int)
     */
    ///{@
    explicit Arg(int val) noexcept;
    explicit Arg(double val) noexcept;
    explicit Arg(const char* val) noexcept;
    explicit Arg(const std::string& val);
    explicit Arg(std::initializer_list<Arg> l);
    template<size_t N>
    explicit Arg(const Arg (&val)[N])
        : Arg(nullptr, val, N)
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
    ///{@
    explicit Arg(const char* key, int val) noexcept;
    explicit Arg(const char* key, double val) noexcept;
    explicit Arg(const char* key, const char* val) noexcept;
    explicit Arg(const char* key, const std::string& val);
    explicit Arg(const char* key, std::initializer_list<Arg> l);
    template<size_t N>
    explicit Arg(const char* key, const Arg (&val)[N])
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
     * \return The type of this argument.
     */
    Type type() const noexcept;

    /**
     * \return The key of this argument, or \c nullptr if it was not set.
     */
    const char* key() const noexcept;

    /**
     * \return The value of this argument if the type matches the requested value,
     *         or 0 otherwise.
     */
    ///{@
    int toInt() const noexcept;
    double toDouble() const noexcept;
    const char* toCString() const noexcept;
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
        T* const buf_;     ///< Data buffer.
        const size_t siz_; ///< Size of buffer.
        size_t cnt_;       ///< Reference count.

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
        ///{@
        explicit Val(int) noexcept;
        explicit Val(double) noexcept;
        explicit Val(const char*) noexcept;
        explicit Val(Ref<char>*) noexcept;
        explicit Val(Ref<Arg>*) noexcept;
        ///@}

        /// Underlying type of value.
        ///{@
        int int_;
        double dbl_;
        const char* chr_;
        char buf_[sizeof(double)];
        Ref<char>* str_;
        Ref<Arg>* arr_;
        ///@}
    };

    /**
     * \brief Type of allocation of argument value.
     */
    enum Alloc : uint8_t
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
    Arg(const char* key, const Arg arg[], size_t size);

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


private:
    Type type_;       ///< Type of argument.
    Alloc alloc_;     ///< Type of value allocation.
    const char* key_; ///< Argument key.
    Val val_;         ///< Argument value.
};


/**
 * \brief Outputs a log argument.
 */
std::ostream& operator<<(std::ostream& os, const Arg& arg);


/**
 * \brief Interface for a generic content handler.
 * User code shall derive this class to implement custom logging.
 */
class Handler
{
public:
    /**
     * \brief Logging function.
     *
     * \param[in] loc Location of the log message.
     * \param[in] args Arguments of the log message.
     * \param[in] num Number of arguments.
     */
    ///{@
    virtual void debug(const Loc& loc, const Arg args[], size_t num) const = 0;
    virtual void info(const Loc& loc, const Arg args[], size_t num) const = 0;
    virtual void warn(const Loc& loc, const Arg args[], size_t num) const = 0;
    virtual void error(const Loc& loc, const Arg args[], size_t num) const = 0;
    virtual void fatal(const Loc& loc, const Arg args[], size_t num) const = 0;
    ///@}

    /// Destructor.
    virtual ~Handler();
};

/**
 * \brief Logging handler which prints every content to stdout/stderr.
 * This is the default installed handler.
 *
 * \see Handler
 */
class StandardHandler : public Handler
{
    /**
     * \brief Logging function which prints the content to stdout/stderr.
     */
    ///{@
    void debug(const Loc&, const Arg[], size_t) const override;
    void info(const Loc&, const Arg[], size_t) const override;
    void warn(const Loc&, const Arg[], size_t) const override;
    void error(const Loc&, const Arg[], size_t) const override;
    void fatal(const Loc&, const Arg[], size_t) const override;
    ///@}
};

/**
 * \brief Logging handler which discards every content.
 * No logging content will be ever shown.
 *
 * \see Handler
 */
class SilentHandler : public Handler
{
    /**
     * \brief Logging function which discards the content.
     */
    ///{@
    void debug(const Loc&, const Arg[], size_t) const override;
    void info(const Loc&, const Arg[], size_t) const override;
    void warn(const Loc&, const Arg[], size_t) const override;
    void error(const Loc&, const Arg[], size_t) const override;
    void fatal(const Loc&, const Arg[], size_t) const override;
    ///@}
};

/**
 * \brief Library-level generic logger.
 *
 * This logger shall use the installed \ref Handler to log any content.
 *
 * \see Handler
 * \see Loc
 * \see Arg
 */
class Logger
{
public:
    /**
     * \brief Installs a custom \ref Handler for any library log content.
     *
     * \param[in] handler A valid pointer to the content \ref Handler.
     *      The ownership of the object is taken.
     */
    static void installContentHandler(Handler* handler);

    /**
     * \brief Logs a content for level, using the installed \ref Handler.
     * In case no handler was installed, then the default one is used.
     * The default handler causes the application to be aborted on a \c fatal log.
     *
     * \param[in] loc Location of the log message.
     * \param[in] args Arguments of the log message.
     * \param[in] num Number of arguments of the log message.
     */
    ///{@
    static void debug(const Loc& loc, const Arg args[], size_t num) noexcept;
    static void info(const Loc& loc, const Arg args[], size_t num) noexcept;
    static void warn(const Loc& loc, const Arg args[], size_t num) noexcept;
    static void error(const Loc& loc, const Arg args[], size_t num) noexcept;
    static void fatal(const Loc& loc, const Arg args[], size_t num) noexcept;
    ///@}


private:
    static std::unique_ptr<Handler> handler_; ///< The user defined log handler.
};
}
}
#endif // LOGGER_H
