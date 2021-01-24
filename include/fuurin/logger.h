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

#include "fuurin/arg.h"

#include <memory>


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
 * \brief Interface for a generic content handler.
 * User code shall derive this class to implement custom logging.
 */
class Handler
{
public:
    /**
     * \brief Logging method.
     *
     * This method shall be thread-safe.
     *
     * \param[in] loc Location of the log message.
     * \param[in] args Arguments of the log message.
     * \param[in] num Number of arguments.
     */
    ///@{
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
     *
     * This method is thread-safe.
     */
    ///@{
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
     *
     * This method is thread-safe.
     */
    ///@{
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
    ///@{
    static void debug(const Loc& loc, const Arg args[], size_t num) noexcept;
    static void info(const Loc& loc, const Arg args[], size_t num) noexcept;
    static void warn(const Loc& loc, const Arg args[], size_t num) noexcept;
    static void error(const Loc& loc, const Arg args[], size_t num) noexcept;
    static void fatal(const Loc& loc, const Arg args[], size_t num) noexcept;
    ///@}


private:
    static std::unique_ptr<Handler> handler_; ///< The user defined log handler.
};

} // namespace log
} // namespace fuurin
#endif // LOGGER_H
