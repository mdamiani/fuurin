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
 * \brief Message that holds the log details.
 */
struct Message
{
    const unsigned int line; ///< Code line where the log happened.
    const char* const file;  ///< File name where the log happened.
    const char* const where; ///< Function that caused the log.
    const char* const what;  ///< Main log message.
};

/**
 * \brief Interface for a generic message handler.
 * User code shall derive this class to implement custom logging.
 */
class Handler
{
public:
    /**
     * \brief Logging function.
     *
     * \param[in] message Message to be logged.
     */
    ///{@
    virtual void debug(const Message& message) = 0;
    virtual void info(const Message& message) = 0;
    virtual void warn(const Message& message) = 0;
    virtual void error(const Message& message) = 0;
    virtual void fatal(const Message& message) = 0;
    ///@}

    /// Destructor.
    virtual ~Handler();
};

/**
 * \brief Library-level generic logger.
 *
 * This logger shall use the installed \ref Handler to log any \ref Message.
 *
 * \see Handler
 * \see Message
 */
class Logger
{
public:
    /**
     * \brief Installs a custom \ref Handler for any library log \ref Message.
     *
     * \param[in] handler A pointer to the message \ref Handler. The ownership of the
     *                    object is taken. Passing \c nullptr causes the default
     *                    message handler to be installed.
     */
    static void installMessageHandler(Handler* handler);

    /**
     * \brief Logs a \ref Message for level, using the installed \ref Handler.
     * In case no handler was installed, then the default one is used.
     * The default handler causes the application to be aborted on a \c fatal message.
     *
     * \param[in] message Message to be logged.
     */
    ///{@
    static void debug(const Message& message);
    static void info(const Message& message);
    static void warn(const Message& message);
    static void error(const Message& message);
    static void fatal(const Message& message);
    ///@}

private:
    static std::unique_ptr<Handler> _handler; ///< The user defined log handler.
};
}
}
#endif // LOGGER_H
