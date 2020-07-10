/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_EVENT_H
#define FUURIN_EVENT_H

#include "fuurin/arg.h"
#include "fuurin/zmqpart.h"


#include <ostream>
#include <string_view>
#include <array>


namespace fuurin {

/**
 * \brief This is an event notified by \ref Runner, or its subclasses.
 *
 * \see Worker::waitForEvent(std::chrono::milliseconds)
 * \see Runner::sendEvent(Event::Type, zmq::Part&&)
 */
class Event
{
public:
    /**
     * \brief Type of event read.
     */
    enum struct Notification
    {
        Discard, ///< The read of event returned an old event.
        Timeout, ///< The read of event timed out.
        Success, ///< The read of event successfully returned an event.

        COUNT, ///< Number of items.
    };

    using type_t = uint8_t; ///< Underlying type of the event \ref Type.

    /**
     * \brief Type of event payload.
     */
    enum struct Type : type_t
    {
        Invalid,         ///< Event is invalid.
        Started,         ///< Event delivered when \ref Runner::start() was acknowledged.
        Stopped,         ///< Event delivered when \ref Runner::stop() was acknowledged.
        Offline,         ///< Event for \ref Worker disconnection.
        Online,          ///< Event for \ref Worker connection.
        Delivery,        ///< Event for any \ref Worker::dispatch(zmq::Part&&).
        SyncBegin,       ///< Event for start of \ref Worker::sync().
        SyncElement,     ///< Event for any element of \ref Worker::sync().
        SyncSuccess,     ///< Event for \ref Worker::sync() success.
        SyncError,       ///< Event for \ref Worker::sync() error.
        SyncDownloadOn,  ///< Event when download of snapshot starts.
        SyncDownloadOff, ///< Event when download of snapshot stops.

        COUNT, ///< Number of items.
    };


public:
    /**
     * \brief Convert event enumeration to string representation.
     *
     * \param[in] v Enum value.
     *
     * \return A representable string.
     */
    ///{@
    static std::string_view toString(Event::Notification v) noexcept;
    static std::string_view toString(Event::Type v) noexcept;
    ///@}


public:
    /**
     * \brief Initializes an \ref Type::Invalid event.
     */
    Event() noexcept;

    /**
     * \brief Initilizes en event with data.
     *
     * \param[in] type Event type.
     * \param[in] notif Event notification type.
     * \param[in] data Event payload.
     */
    Event(Type type, Notification notif, zmq::Part&& data = zmq::Part{}) noexcept;

    /**
     * \brief Destructor.
     */
    virtual ~Event() noexcept;

    /**
     * \return The type of this event.
     */
    Type type() const noexcept;

    /**
     * \return The notification of this event.
     */
    Notification notification() const noexcept;

    /**
     * \return The payload of this event.
     */
    ///{@
    zmq::Part& payload() noexcept;
    const zmq::Part& payload() const noexcept;
    ///@}

    /**
     * \brief Converts this event to log arguments.
     *
     * \return An array with <\ref type(), \ref notification(), bytes of \ref payload()>
     */
    std::array<log::Arg, 3> toArgs() const;


protected:
    const Type type_;          ///< Eventy type.
    const Notification notif_; ///< Event notification.
    zmq::Part payld_;          ///< Event payload.
};


///< Streams to printable form a \ref Event::Notification value.
std::ostream& operator<<(std::ostream& os, const Event::Notification& v);

///< Streams to printable form a \ref Event::Type value.
std::ostream& operator<<(std::ostream& os, const Event::Type& v);

///< Streams to printable form a \ref Event value.
std::ostream& operator<<(std::ostream& os, const Event& v);

} // namespace fuurin

#endif // FUURIN_EVENT_H
