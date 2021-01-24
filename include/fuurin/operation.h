/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_OPERATION_H
#define FUURIN_OPERATION_H

#include "fuurin/arg.h"
#include "fuurin/zmqpart.h"


#include <ostream>
#include <string_view>
#include <array>


namespace fuurin {


/**
 * \brief This is an operation supported by \ref Runner, or its subclasses.
 *
 * \see Runner:sendOperation(Operation, zmq::Part&&)
 * \see Session::operationReady(Operation, zmq::Part&&)
 * \see Session::recvOperation()
 */
class Operation
{
public:
    using type_t = uint8_t;  ///< Underlying type of the event \ref Type.
    using notif_t = uint8_t; ///< Underlying type of the event \ref Notification.

    /**
     * \brief Type of operation read.
     */
    enum struct Notification : notif_t
    {
        Discard, ///< The read of operation returned an old event.
        Success, ///< The read of operation successfully returned an event.

        COUNT, ///< Number of items.
    };

    /**
     * \brief Type of operation.
     */
    enum struct Type : type_t
    {
        Invalid,  ///< Invalid command.
        Start,    ///< Operation for \ref Runner::start().
        Stop,     ///< Operation for \ref Runner::stop().
        Dispatch, ///< Operation for \ref Worker::dispatch().
        Sync,     ///< Operation for \ref Worker::sync().

        COUNT, ///< Number of operations.
    };


public:
    /**
     * \brief Convert operation enumeration to string representation.
     *
     * \param[in] v Enum value.
     *
     * \return A representable string.
     */
    ///@{
    static std::string_view toString(Operation::Notification v) noexcept;
    static std::string_view toString(Operation::Type v) noexcept;
    ///@}


public:
    /**
     * \brief Initializes an \ref Type::Invalid operation.
     */
    Operation() noexcept;

    /**
     * \brief Initilizes an operation with data.
     *
     * \param[in] type Operation type.
     * \param[in] notif Event notification type.
     * \param[in] data Operation payload.
     */
    ///@{
    Operation(Type type, Notification notif, const zmq::Part& data) noexcept;
    Operation(Type type, Notification notif, zmq::Part&& data = zmq::Part{}) noexcept;
    ///@}

    /**
     * \brief Destructor.
     */
    virtual ~Operation() noexcept;

    /**
     * \return The type of this operation.
     */
    Type type() const noexcept;

    /**
     * \return The notification of this operation.
     */
    Notification notification() const noexcept;

    /**
     * \return The payload of this operation.
     */
    ///@{
    zmq::Part& payload() noexcept;
    const zmq::Part& payload() const noexcept;
    ///@}

    /**
     * \brief Modifies passed value.
     * \param[in] v Operation type.
     */
    Operation& withType(Type v);

    /**
     * \brief Modifies passed value.
     * \param[in] v Operation notification.
     */
    Operation& withNotification(Notification v);

    /**
     * \brief Modifies passed value.
     * \param[in] v Operation payload.
     */
    ///@{
    Operation& withPayload(const zmq::Part& v);
    Operation& withPayload(zmq::Part&& v);
    ///@}


    /**
     * \brief Converts this operation to log arguments.
     *
     * \return An array with <\ref type(), \ref notification(), bytes of \ref payload()>.
     */
    std::array<log::Arg, 3> toArgs() const;


public:
    /**
     * \brief Creates new operation from a \ref zmq::PartMulti packed \ref zmq::Part.
     *
     * \param[in] part Packed operation.
     *
     * \return A new instance of operation.
     *
     * \see zmq::PartMulti::unpack(const Part&)
     */
    static Operation fromPart(const zmq::Part& part);

    /**
     * \brief Converts this operation to a \ref zmq::PartMulti packed \ref zmq::Part.
     *
     * \return A packed \ref zmq::Part representing this operation.
     *
     * \see zmq::PartMulti::pack(Args&&...)
     */
    zmq::Part toPart() const;


protected:
    Type type_;          ///< Operation type.
    Notification notif_; ///< Event notification.
    zmq::Part payld_;    ///< Operation payload.
};


///< Streams to printable form a \ref Operation::Notification value.
std::ostream& operator<<(std::ostream& os, const Operation::Notification& v);

///< Streams to printable form a \ref Operation::Type value.
std::ostream& operator<<(std::ostream& os, const Operation::Type& v);

///< Streams to printable form a \ref Operation value.
std::ostream& operator<<(std::ostream& os, const Operation& v);

} // namespace fuurin

#endif // FUURIN_OPERATION_H
