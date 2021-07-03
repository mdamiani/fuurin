/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_C_TOPIC_H
#define FUURIN_C_TOPIC_H

#include "fuurin/c/cuuid.h"

#include <stddef.h>


#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct CTopic CTopic;

    typedef enum TopicType_t
    {
        TopicState,
        TopicEvent,
    } TopicType_t;

    CUuid CTopic_brokerUuid(CTopic* t);
    CUuid CTopic_workerUuid(CTopic* t);
    unsigned long long CTopic_seqNum(CTopic* t);
    TopicType_t CTopic_type(CTopic* t);
    const char* CTopic_name(CTopic* t);
    const char* CTopic_data(CTopic* t);
    size_t CTopic_size(CTopic* t);

#ifdef __cplusplus
}
#endif

#endif // FUURIN_C_TOPIC_H
