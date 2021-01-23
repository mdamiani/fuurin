/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_BROKERCONFIG_H
#define FUURIN_BROKERCONFIG_H

#include "fuurin/uuid.h"

#include <vector>
#include <string>


namespace fuurin {

namespace zmq {
class Part;
} // namespace zmq


/**
 * \brief Broker run configuration to be used upon \ref Broker::start().
 */
struct BrokerConfig
{
    ///< Worker Uuid.
    Uuid uuid;

    ///< List of endpoints.
    ///@{
    std::vector<std::string> endpDelivery;
    std::vector<std::string> endpDispatch;
    std::vector<std::string> endpSnapshot;
    ///@}

    /**
     * \brief Comparison operator.
     * \param[in] rhs Another config.
     */
    ///@{
    bool operator==(const BrokerConfig& rhs) const;
    bool operator!=(const BrokerConfig& rhs) const;
    ///@}

    /**
     * \brief Creates new topic from a \ref zmq::PartMulti packed \ref zmq::Part.
     *
     * \param[in] part Packed configuration.
     *
     * \return A new configuration.
     *
     * \see zmq::PartMulti::unpack(const Part&)
     */
    static BrokerConfig fromPart(const zmq::Part& part);

    /**
     * \brief Converts this configuration to a \ref zmq::PartMulti packed \ref zmq::Part.
     *
     * \return A packed \ref zmq::Part representing this configuration.
     *
     * \see zmq::PartMulti::pack(Args&&...)
     */
    zmq::Part toPart() const;
};


///< Streams to printable form.
std::ostream& operator<<(std::ostream& os, const BrokerConfig& cc);

} // namespace fuurin

#endif // FUURIN_BROKERCONFIG_H
