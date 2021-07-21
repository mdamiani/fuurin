/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/c/cevent.h"
#include "fuurin/event.h"
#include "fuurin/topic.h"
#include "ceventd.h"
#include "ctopicd.h"


using namespace fuurin;


EventType_t CEvent_type(CEvent* ev)
{
    switch (c::getPrivD(ev)->ev.type()) {
    case Event::Type::Invalid:
    default:
        return EventInvalid;

    case Event::Type::Started:
        return EventStarted;

    case Event::Type::Stopped:
        return EventStopped;

    case Event::Type::Offline:
        return EventOffline;

    case Event::Type::Online:
        return EventOnline;

    case Event::Type::Delivery:
        return EventDelivery;

    case Event::Type::SyncRequest:
        return EventSyncRequest;

    case Event::Type::SyncBegin:
        return EventSyncBegin;

    case Event::Type::SyncElement:
        return EventSyncElement;

    case Event::Type::SyncSuccess:
        return EventSyncSuccess;

    case Event::Type::SyncError:
        return EventSyncError;

    case Event::Type::SyncDownloadOn:
        return EventSyncDownloadOn;

    case Event::Type::SyncDownloadOff:
        return EventSyncDownloadOff;
    }

    return EventInvalid;
}


EventNotif_t CEvent_notif(CEvent* ev)
{
    switch (c::getPrivD(ev)->ev.notification()) {
    case Event::Notification::Discard:
    default:
        return EventDiscard;

    case Event::Notification::Timeout:
        return EventTimeout;

    case Event::Notification::Success:
        return EventSuccess;
    }

    return EventDiscard;
}


CTopic* CEvent_topic(CEvent* ev)
{
    try {
        auto* evd = c::getPrivD(ev);
        evd->tp = Topic::fromPart(evd->ev.payload());
        return c::getOpaque(&evd->tp);

    } catch (const std::exception&) {
        return nullptr;
    }
}
