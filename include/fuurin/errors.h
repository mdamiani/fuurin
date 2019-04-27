/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ERRORS_H
#define ERRORS_H

#include "fuurin/logger.h"

#include <exception>

namespace fuurin {
namespace err {

/**
 * \brief Base class of library errors.
 */
class Error : public std::exception
{
public:
    /**
     * \brief Initializes this error with string description and arguments.
     * \param[in] loc Location of the error.
     * \param[in] what Error description.
     * \param[in] reason Error specific reason.
     * \param[in] arg Error arguments.
     */
    explicit Error(const log::Loc& loc, const char* what, const char* reason,
        const log::Arg& arg = log::Arg{}) noexcept;

    /**
     * \brief Initializes this error with error code and arguments.
     * \param[in] loc Location of the error.
     * \param[in] what Error description.
     * \param[in] ec Error code (reason).
     * \param[in] arg Error arguments.
     */
    explicit Error(const log::Loc& loc, const char* what, int ec,
        const log::Arg& arg = log::Arg{}) noexcept;

    /**
     * \return Error explanatory string.
     */
    const char* what() const noexcept override;

    /**
     * \return Error specific reason.
     */
    const char* reason() const noexcept;

    /**
     * \return Whether this error was constructed with an error code.
     */
    bool hasCode() const noexcept;

    /**
     * \return The code of this error, or 0 if it was not
     *         constructed with an error code.
     * \see hasCode()
     */
    int code() const noexcept;

    /**
     * \return Location of the error.
     */
    const log::Loc& loc() const noexcept;

    /**
     * \return Error arguments.
     */
    const log::Arg& arg() const noexcept;


private:
    const log::Loc loc_;       ///< Location of this error.
    const char* const what_;   ///< Name of this error.
    const char* const reason_; ///< Name of this error.
    const bool sys_;           ///< Whether a system error code was passed.
    const int ec_;             ///< Code of this error.
    const log::Arg arg_;       ///< Error arguments.
};

#define DECL_ERROR(Name) \
    struct Name : Error \
    { \
        using Error::Error; \
    };

DECL_ERROR(ZMQContextCreateFailed)
DECL_ERROR(ZMQSocketCreateFailed)
DECL_ERROR(ZMQSocketOptionSetFailed)
DECL_ERROR(ZMQSocketOptionGetFailed)
DECL_ERROR(ZMQSocketConnectFailed)
DECL_ERROR(ZMQSocketBindFailed)

#undef DECL_ERROR

#define ERROR(type, error, reason, ...) \
    fuurin::err::type({__FILE__, __LINE__}, error, reason, ##__VA_ARGS__)
} // namespace err
} // namespace fuurin

#endif // ERRORS_H
