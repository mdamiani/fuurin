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
#include <ostream>


namespace fuurin {
namespace err {

/**
 * \brief Base class of library errors.
 */
class Error : public std::exception
{
public:
    /**
     * \brief Initializes an empty default error.
     *
     * \see empty()
     */
    explicit Error() noexcept;

    /**
     * \brief Initializes this error with string description and arguments.
     * \param[in] loc Location of the error.
     * \param[in] what Error description.
     * \param[in] arg Error arguments.
     */
    explicit Error(const log::Loc& loc, const char* what, const log::Arg& arg = log::Arg{}) noexcept;

    /**
     * \brief Destructor.
     */
    virtual ~Error() noexcept;

    /**
     * \return Whether this is a default empty error.
     */
    bool empty() const noexcept;

    /**
     * \return Error explanatory string.
     */
    const char* what() const noexcept override;

    /**
     * \return Location of the error.
     */
    const log::Loc& loc() const noexcept;

    /**
     * \return Error arguments.
     */
    const log::Arg& arg() const noexcept;


private:
    const bool empty_;       ///< Whether this error is empty.
    const char* const what_; ///< Name of this error.
    const log::Loc loc_;     ///< Location of this error.
    const log::Arg arg_;     ///< Error arguments.
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
DECL_ERROR(ZMQSocketSendFailed)
DECL_ERROR(ZMQSocketRecvFailed)
DECL_ERROR(ZMQSocketGroupFailed)
DECL_ERROR(ZMQPartCreateFailed)
DECL_ERROR(ZMQPartMoveFailed)
DECL_ERROR(ZMQPartCopyFailed)
DECL_ERROR(ZMQPartAccessFailed)
DECL_ERROR(ZMQPartRoutingIDFailed)
DECL_ERROR(ZMQPartGroupFailed)
DECL_ERROR(ZMQPollerCreateFailed)
DECL_ERROR(ZMQPollerAddSocketFailed)
DECL_ERROR(ZMQPollerWaitFailed)

#undef DECL_ERROR

#define ERROR(type, reason, ...) \
    fuurin::err::type({__FILE__, __LINE__}, reason, ##__VA_ARGS__)


std::ostream& operator<<(std::ostream& os, const Error& e);

} // namespace err
} // namespace fuurin

#endif // ERRORS_H
