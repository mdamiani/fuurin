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


namespace fuurin {
namespace log {

/**
 * \brief Content that holds the log details.
 */
struct Content
{
    const unsigned int line; ///< Code line where the log happened.
    const char* const file;  ///< File name where the log happened.
    const char* const where; ///< Function that caused the log.
    const char* const what;  ///< Main log content.
};

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
     * \param[in] c Content to be logged.
     */
    ///{@
    virtual void debug(const Content& c) const = 0;
    virtual void info(const Content& c) const = 0;
    virtual void warn(const Content& c) const = 0;
    virtual void error(const Content& c) const = 0;
    virtual void fatal(const Content& c) const = 0;
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
     * \brief Logging function which prints the \c content to stdout/stderr.
     */
    ///{@
    void debug(const Content&) const override;
    void info(const Content&) const override;
    void warn(const Content&) const override;
    void error(const Content&) const override;
    void fatal(const Content&) const override;
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
    void debug(const Content&) const override;
    void info(const Content&) const override;
    void warn(const Content&) const override;
    void error(const Content&) const override;
    void fatal(const Content&) const override;
    ///@}
};

/**
 * \brief Library-level generic logger.
 *
 * This logger shall use the installed \ref Handler to log any \ref Content.
 *
 * \see Handler
 * \see Content
 */
class Logger
{
public:
    /**
     * \brief Installs a custom \ref Handler for any library log \ref Content.
     *
     * \param[in] handler A valid pointer to the content \ref Handler.
     *      The ownership of the object is taken.
     */
    static void installContentHandler(Handler* handler);

    /**
     * \brief Logs a \ref Content for level, using the installed \ref Handler.
     * In case no handler was installed, then the default one is used.
     * The default handler causes the application to be aborted on a \c fatal log.
     *
     * \param[in] c Content to be logged.
     */
    ///{@
    static void debug(const Content& c) noexcept;
    static void info(const Content& c) noexcept;
    static void warn(const Content& c) noexcept;
    static void error(const Content& c) noexcept;
    static void fatal(const Content& c) noexcept;
    ///@}

private:
    static std::unique_ptr<Handler> handler_; ///< The user defined log handler.
};
}
}
#endif // LOGGER_H
