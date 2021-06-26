/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_C_UUID_H
#define FUURIN_C_UUID_H


#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct CUuid
    {
        unsigned char bytes[16];
    } CUuid;

    CUuid CUuid_createNullUuid();
    CUuid CUuid_createRandomUuid();
    CUuid CUuid_createDnsUuid(const char* name);
    CUuid CUuid_createUrlUuid(const char* name);
    CUuid CUuid_createOidUuid(const char* name);
    CUuid CUuid_createX500dnUuid(const char* name);

#ifdef __cplusplus
}
#endif

#endif // FUURIN_C_UUID_H
