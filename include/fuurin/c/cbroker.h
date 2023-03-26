/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_C_BROKER_H
#define FUURIN_C_BROKER_H

#include "fuurin/c/cuuid.h"

#include <stdbool.h>


#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * \brief C type for broker.
     */
    typedef struct CBroker CBroker;

    /**
     * \brief Creates a new instance of broker.
     *
     * \param[in] id Broker's uuid.
     * \param[in] name Broker's name.
     *
     * \return Pointer to the new broker, or nullptr in case of errors.
     */
    CBroker* CBroker_new(CUuid* id, const char* name);

    /**
     * \brief Deletes a C broker object.
     * \param[in] b Pointer to a C broker object.
     */
    void CBroker_delete(CBroker* b);

    /**
     * \return Broker name.
     * \param[in] b Pointer to a C broker object.
     */
    const char* CBroker_name(CBroker* b);

    /**
     * \return Broker uuid.
     * \param[in] b Pointer to a C broker object.
     */
    CUuid CBroker_uuid(CBroker* b);

    /**
     * \brief Adds an endpoint triplet.
     * \param[in] b Pointer to a C broker object.
     * \param[in] delivery Enpoint for delivery.
     * \param[in] dispatch Enpoint for dispatch.
     * \param[in] snapshot Enpoint for snapshot.
     */
    void CBroker_addEndpoints(CBroker* b, const char* delivery, const char* dispatch, const char* snapshot);

    /**
     * \brief Clears every endpoints.
     * \param[in] b Pointer to a C broker object.
     */
    void CBroker_clearEndpoints(CBroker* b);

    /**
     * \return The first configured endpoint, or \c nullptr if empty.
     * \param[in] b Pointer to a C broker object.
     */
    ///@{
    const char* CBroker_endpointDelivery(CBroker* b);
    const char* CBroker_endpointDispatch(CBroker* b);
    const char* CBroker_endpointSnapshot(CBroker* b);
    ///@}

    /**
     * \brief Starts the broker.
     * \param[in] b Pointer to a C broker object.
     */
    void CBroker_start(CBroker* b);

    /**
     * \brief Stops the broker.
     * \param[in] b Pointer to a C broker object.
     */
    void CBroker_stop(CBroker* b);

    /**
     * \brief Wait the broker's thread to be finished.
     * \param[in] b Pointer to a C broker object.
     */
    void CBroker_wait(CBroker* b);

    /**
     * \return Whether the broker is running.
     * \param[in] b Pointer to a C broker object.
     */
    bool CBroker_isRunning(CBroker* b);

#ifdef __cplusplus
}
#endif

#endif // FUURIN_C_BROKER_H
