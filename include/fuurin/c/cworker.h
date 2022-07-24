/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_C_WORKER_H
#define FUURIN_C_WORKER_H

#include "fuurin/c/cuuid.h"
#include "fuurin/c/cevent.h"

#include <stdbool.h>


#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * \brief C type for worker.
     */
    typedef struct CWorker CWorker;

    /**
     * \brief Creates a new instance of worker.
     *
     * \param[in] id Worker's uuid.
     * \param[in] seqn Worker's initial sequence number.
     * \param[in] name Worker's name.
     *
     * \return Pointer to the new worker, or nullptr in case of errors.
     */
    CWorker* CWorker_new(CUuid* id, unsigned long long seqn, const char* name);

    /**
     * \brief Deletes a C worker object.
     * \param[in] w Pointer to a C worker object.
     */
    void CWorker_delete(CWorker* w);

    /**
     * \return Worker name.
     * \param[in] w Pointer to a C worker object.
     */
    const char* CWorker_name(CWorker* w);

    /**
     * \return Worker uuid.
     * \param[in] w Pointer to a C worker object.
     */
    CUuid CWorker_uuid(CWorker* w);

    /**
     * \return Worker sequence number.
     * \param[in] w Pointer to a C worker object.
     */
    unsigned long long CWorker_seqNum(CWorker* w);

    /**
     * \brief Adds an endpoint triplet.
     * \param[in] w Pointer to a C worker object.
     * \param[in] delivery Enpoint for delivery.
     * \param[in] dispatch Enpoint for dispatch.
     * \param[in] snapshot Enpoint for snapshot.
     */
    void CWorker_addEndpoints(CWorker* w, const char* delivery, const char* dispatch, const char* snapshot);

    /**
     * \brief Clears every endpoints.
     * \param[in] w Pointer to a C worker object.
     */
    void CWorker_clearEndpoints(CWorker* w);

    /**
     * \return The first configured endpoint, or \c nullptr if empty.
     * \param[in] w Pointer to a C worker object.
     */
    ///@{
    const char* CWorker_endpointDelivery(CWorker* w);
    const char* CWorker_endpointDispatch(CWorker* w);
    const char* CWorker_endpointSnapshot(CWorker* w);
    ///@}

    /**
     * \brief Starts the worker.
     * \param[in] w Pointer to a C worker object.
     */
    void CWorker_start(CWorker* w);

    /**
     * \brief Stops the worker.
     * \param[in] w Pointer to a C worker object.
     */
    void CWorker_stop(CWorker* w);

    /**
     * \brief Wait the worker's thread to be finished.
     * \param[in] w Pointer to a C worker object.
     */
    void CWorker_wait(CWorker* w);

    /**
     * \return Whether the worker is running.
     * \param[in] w Pointer to a C worker object.
     */
    bool CWorker_isRunning(CWorker* w);

    /**
     * \brief Sets reception of every topic.
     * \param[in] w Pointer to a C worker object.
     */
    void CWorker_setTopicsAll(CWorker* w);

    /**
     * \brief Add reception of specific topics.
     * \param[in] w Pointer to a C worker object.
     * \param[in] name Topic name.
     */
    void CWorker_addTopicsNames(CWorker* w, const char* name);

    /**
     * \brief Sets the list of topics to be received to emtpy.
     * \param[in] w Pointer to a C worker object.
     */
    void CWorker_clearTopicsNames(CWorker* w);

    /**
     * \return Whether to receive every topic.
     * \param[in] w Pointer to a C worker object.
     */
    bool CWorker_topicsAll(CWorker* w);

    /**
     * \return The first topic name to be received, or \c nullptr.
     * \param[in] w Pointer to a C worker object.
     */
    const char* CWorker_topicsNames(CWorker* w);

    /**
     * \brief Dispatches a new message to the broker(s).
     * \param[in] w Pointer to a C worker object.
     * \param[in] name Topic name.
     * \param[in] data Topic data payload.
     * \param[in] size Topic data size.
     * \param[in] type Topic type.
     */
    void CWorker_dispatch(CWorker* w, const char* name, const char* data, size_t size, TopicType_t type);

    /**
     * \brief Syncs with the broker(s).
     * \param[in] w Pointer to a C worker object.
     */
    void CWorker_sync(CWorker* w);

    /**
     * \brief Waits from a broker(s) event.
     *
     * \param[in] w Pointer to a C worker object.
     * \param[in] timeout_ms Waiting timeout, in milliseconds.
     *
     * \return Pointer to the received event.
     *         Pointer remains valid until the next call to this function.
     *         It returns an invalid event in case of timeout.
     *         It returns \c nullptr in case of errors.
     */
    CEvent* CWorker_waitForEvent(CWorker* w, long timeout_ms);

    /**
     * \brief Waits for a specific worker's event.
     *
     * \param[in] w Pointer to a C worker object.
     * \param[in] timeout_ms Waiting timeout, in milliseconds.
     *
     * \return Whether the requested event was received before timeout,
     *         or \c false in case of errors.
     */
    ///@{
    bool CWorker_waitForStarted(CWorker* w, long timeout_ms);
    bool CWorker_waitForStopped(CWorker* w, long timeout_ms);
    bool CWorker_waitForOnline(CWorker* w, long timeout_ms);
    bool CWorker_waitForOffline(CWorker* w, long timeout_ms);
    ///@}

    /**
     * \brief Waits for a topic.
     *
     * \param[in] w Pointer to a C worker object.
     * \param[in] timeout_ms Waiting timeout, in milliseconds.
     *
     * \return Pointer to the received topic.
     *         Pointer remains valid until the next call to this function or \ref CEvent_topic().
     *         It returns \c nullptr in case of timeout or errors.
     */
    CTopic* CWorker_waitForTopic(CWorker* w, long timeout_ms);

    /**
     * \brief Gets the file descriptor of the events socket.
     *
     * The returned file descriptor can be used to wait for
     * events with an existing event loop.
     *
     * When the file descriptor is readable,
     * \ref CWorker_waitForEvent shall be used passing
     * a 0s timeout, until no more events are available
     * (that is a \ref fuurin::Event::Notification::Timeout notification).
     *
     * This function shall not fail.
     *
     * \return The file descriptor to poll.
     *
     * \see CWorker_waitForEvent(CWorker*, long)
     */
    int CWorker_eventFD(CWorker* w);

#ifdef __cplusplus
}
#endif

#endif // FUURIN_C_WORKER_H
