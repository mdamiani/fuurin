/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_C_EVENT_H
#define FUURIN_C_EVENT_H

#include "fuurin/c/ctopic.h"


#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct CEvent CEvent;

    typedef enum EventType_t
    {
        EventInvalid,
        EventStarted,
        EventStopped,
        EventOffline,
        EventOnline,
        EventDelivery,
        EventSyncRequest,
        EventSyncBegin,
        EventSyncElement,
        EventSyncSuccess,
        EventSyncError,
        EventSyncDownloadOn,
        EventSyncDownloadOff,
    } EventType_t;

    typedef enum EventNotif_t
    {
        EventDiscard,
        EventTimeout,
        EventSuccess,
    } EventNotif_t;

    EventType_t CEvent_type(CEvent* ev);
    EventNotif_t CEvent_notif(CEvent* ev);
    CTopic* CEvent_topic(CEvent* ev);

#ifdef __cplusplus
}
#endif

#endif // FUURIN_C_EVENT_H
