/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_TOPIC_H
#define FUURIN_TOPIC_H

#include "fuurin/uuid.h"
#include "fuurin/zmqpart.h"

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <ostream>


namespace fuurin {

/**
 * \brief Basic payload to transfer.
 */
class Topic
{
public:
    /// Payload data type.
    using Data = zmq::Part;

    /// Payload sequence number data type.
    using SeqN = uint64_t;

    /**
     * \brief Payload namae data type.
     *
     * The name has a maximum limit of 16 characters.
     */
    class Name final
    {
    public:
        /**
         * \brief Initilizes an empty name.
         */
        Name();

        /**
         * \brief Initializes a name from a string.
         *
         * The name will be truncated to \ref capacity() characters,
         * when the passed value exceeds that value.
         *
         * \param[in] str Value to initialize.
         */
        ///{@
        Name(std::string_view str);
        Name(const std::string& str);
        ///@}

        /**
         * \return The maximum number of characters.
         */
        constexpr size_t capacity() const noexcept;

        /**
         * \return The size of name.
         */
        size_t size() const noexcept;

        /**
         * \brief Whether this name is emtpy.
         */
        bool empty() const noexcept;

        /**
         * \brief Converts this name to string.
         */
        ///{@
        operator std::string_view() const;
        operator std::string() const;
        ///@}

        /**
         * \brief Comparison operator.
         * \param[in] rhs Another name.
         */
        ///{@
        bool operator==(const Name& rhs) const;
        bool operator!=(const Name& rhs) const;
        ///@}


    private:
        size_t sz_;               ///< Actual size of the name.
        std::array<char, 15> dd_; ///< Backing array of the name.
    };


public:
    /**
     * \brief Inizializes an empty topic.
     */
    Topic();

    /**
     * \brief Initializes a topic with data.
     *
     * \param[in] broker Broker uuid.
     * \param[in] worker Worker uuid.
     * \param[in] seqnum Sequence number.
     * \param[in] name Topic name.
     * \param[in] data Topic data.
     */
    ///{@
    Topic(const Uuid& broker, const Uuid& worker, const SeqN& seqn, const Name& name, const Data& data);
    Topic(Uuid&& broker, Uuid&& worker, SeqN&& seqn, Name&& name, Data&& data);
    ///@}

    /**
     * \brief Destructor.
     */
    virtual ~Topic() noexcept;

    /**
     * \return Broker uuid.
     */
    ///{@
    const Uuid& broker() const noexcept;
    Uuid& broker() noexcept;
    ///@}

    /**
     * \return Worker uuid.
     */
    ///{@
    const Uuid& worker() const noexcept;
    Uuid& worker() noexcept;
    ///@}

    /**
     * \return Sequence number.
     */
    ///{@
    const SeqN& seqNum() const noexcept;
    SeqN& seqNum() noexcept;
    ///@}

    /**
     * \return Topic's name.
     */
    ///{@
    const Name& name() const noexcept;
    Name& name() noexcept;
    ///@}

    /**
     * \return Topic's data.
     */
    ///{@
    const Data& data() const noexcept;
    Data& data() noexcept;
    ///@}

    /**
     * \brief Modifies this topic with passed value.
     * \param[in] v Broker uuid.
     */
    ///{@
    Topic& withBroker(const Uuid& v);
    Topic& withBroker(Uuid&& v);
    ///@}

    /**
     * \brief Modifies this topic with passed value.
     * \param[in] v Worker uuid.
     */
    ///{@
    Topic& withWorker(const Uuid& v);
    Topic& withWorker(Uuid&& v);
    ///@}

    /**
     * \brief Modifies this topic with passed value.
     * \param[in] v Sequence number.
     */
    ///{@
    Topic& withSeqNum(const SeqN& v);
    Topic& withSeqNum(SeqN&& v);
    ///@}

    /**
     * \brief Modifies this topic with passed value.
     * \param[in] v Topic name.
     */
    ///{@
    Topic& withName(const Name& v);
    Topic& withName(Name&& v);
    ///@}

    /**
     * \brief Modifies this topic with passed value.
     * \param[in] v Topic data.
     */
    ///{@
    Topic& withData(const Data& v);
    Topic& withData(Data&& v);
    ///@}

    /**
     * \brief Comparison operator.
     * \param[in] rhs Another topic.
     */
    ///{@
    bool operator==(const Topic& rhs) const;
    bool operator!=(const Topic& rhs) const;
    ///@}


public:
    /**
     * \brief Creates new topic from a \ref zmq::PartMulti packed \ref zmq::Part.
     *
     * \param[in] part Packed topic.
     *
     * \return A new instance of topic.
     *
     * \see zmq::PartMulti::unpack(const Part&)
     */
    static Topic fromPart(const zmq::Part& part);

    /**
     * \brief Converts this topic to a \ref zmq::PartMulti packed \ref zmq::Part.
     *
     * \return A packed \ref zmq::Part representing this topic.
     *
     * \see zmq::PartMulti::pack(Args&&...)
     */
    zmq::Part toPart() const;


private:
    Uuid broker_; ///< Broker uuid.
    Uuid worker_; ///< Worker uuid.
    SeqN seqn_;   ///< Sequence number.
    Name name_;   ///< Topic name.
    Data data_;   ///< Topic data.
};


///< Streams to printable form a \ref SyncMachine::State value.
std::ostream& operator<<(std::ostream& os, const Topic& t);

} // namespace fuurin


namespace std {

/**
 * \brief Makes \ref Topic::Name hashable.
 */
template<>
struct hash<fuurin::Topic::Name>
{
    size_t operator()(const fuurin::Topic::Name& n) const;
};

} // namespace std

#endif // FUURIN_TOPIC_H
