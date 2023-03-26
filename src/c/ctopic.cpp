/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/c/ctopic.h"
#include "fuurin/topic.h"
#include "ctopicd.h"
#include "cutils.h"

#include <string_view>


using namespace fuurin;


CUuid CTopic_brokerUuid(CTopic* t)
{
    return c::uuidConvert(c::getPrivD(t)->broker());
}


CUuid CTopic_workerUuid(CTopic* t)
{
    return c::uuidConvert(c::getPrivD(t)->worker());
}


unsigned long long CTopic_seqNum(CTopic* t)
{
    return c::getPrivD(t)->seqNum();
}


TopicType_t CTopic_type(CTopic* t)
{
    switch (c::getPrivD(t)->type()) {
    case Topic::Event:
        return TopicEvent;
    case Topic::State:
    default:
        return TopicState;
    }
}


const char* CTopic_name(CTopic* t)
{
    return std::string_view(c::getPrivD(t)->name()).data();
}


const char* CTopic_data(CTopic* t)
{
    return c::getPrivD(t)->data().data();
}


size_t CTopic_size(CTopic* t)
{
    return c::getPrivD(t)->data().size();
}
