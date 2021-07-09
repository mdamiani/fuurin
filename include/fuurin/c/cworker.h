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


#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct CWorker CWorker;

    CWorker* CWorker_new(CUuid id, unsigned long long seqn, const char* name);
    void CWorker_delete(CWorker* w);

    const char* CWorker_name(CWorker* w);
    CUuid CWorker_uuid(CWorker* w);
    unsigned long long CWorker_seqNum(CWorker* w);

    void CWorker_addEndpoints(CWorker* w, const char* delivery, const char* dispatch, const char* snapshot);
    void CWorker_clearEndpoints(CWorker* w);

    const char* CWorker_endpointDelivery(CWorker* w);
    const char* CWorker_endpointDispatch(CWorker* w);
    const char* CWorker_endpointSnapshot(CWorker* w);

    void CWorker_start(CWorker* w);
    void CWorker_stop(CWorker* w);
    void CWorker_wait(CWorker* w);
    bool CWorker_isRunning(CWorker* w);


    void CWorker_setTopicsAll(CWorker* w);
    void CWorker_addTopicsNames(CWorker* w, const char* name);
    void CWorker_clearTopicsNames(CWorker* w);
    bool CWorker_topicsAll(CWorker* w);
    const char* CWorker_topicsNames(CWorker* w);


    void CWorker_dispatch(CWorker* w, const char* name, const char* data, size_t size, TopicType_t type);
    void CWorker_sync(CWorker* w);

    CEvent* CWorker_waitForEvent(CWorker* w, unsigned long timeout_ms);
    bool CWorker_waitForStarted(CWorker* w, unsigned long timeout_ms);
    bool CWorker_waitForStopped(CWorker* w, unsigned long timeout_ms);
    bool CWorker_waitForOnline(CWorker* w, unsigned long timeout_ms);
    bool CWorker_waitForOffline(CWorker* w, unsigned long timeout_ms);
    CTopic* CWorker_waitForTopic(CWorker* w, unsigned long timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // FUURIN_C_WORKER_H
