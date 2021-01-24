/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_SESSIONENV_H
#define FUURIN_SESSIONENV_H

#include <cstdint>
#include <string_view>


namespace fuurin {

/**
 * \brief Session constants.
 */
struct SessionEnv
{
    ///< Type of session execution token.
    using token_t = uint8_t;

    ///< Broker publish group for keepalives.
    static constexpr std::string_view BrokerHugz{"HUGZ"};
    ///< Worker publish group for announcemets.
    static constexpr std::string_view WorkerHugz{"HUGZ"};
    ///< Broker publish group for delivery.
    static constexpr std::string_view BrokerUpdt{"UPDT"};
    ///< Worker publish group for dispatch.
    static constexpr std::string_view WorkerUpdt{"UPDT"};

    ///< Broker sync request.
    static constexpr std::string_view BrokerSyncReqst{"SYNC"};
    ///< Broker sync acknowledgement.
    static constexpr std::string_view BrokerSyncBegin{"BEGN"};
    ///< Broker sync topic.
    static constexpr std::string_view BrokerSyncElemn{"ELEM"};
    ///< Broker sync complete.
    static constexpr std::string_view BrokerSyncCompl{"SONC"};
};

} // namespace fuurin

#endif // FUURIN_SESSIONENV_H
