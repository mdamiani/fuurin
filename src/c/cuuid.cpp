/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/c/cuuid.h"
#include "fuurin/uuid.h"

#include <string_view>
#include <algorithm>


namespace {
CUuid copyFromUuid(const fuurin::Uuid& id)
{
    CUuid ret;

    static_assert(std::tuple_size_v<fuurin::Uuid::Bytes> == sizeof(CUuid::bytes));
    std::copy_n(id.bytes().data(), id.size(), ret.bytes);

    return ret;
}
} // namespace


CUuid CUuid_createNullUuid()
{
    return copyFromUuid(fuurin::Uuid());
}


CUuid CUuid_createRandomUuid()
{
    return copyFromUuid(fuurin::Uuid::createRandomUuid());
}


CUuid CUuid_createDnsUuid(const char* name)
{
    return copyFromUuid(fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::Dns, std::string_view(name)));
}


CUuid CUuid_createUrlUuid(const char* name)
{
    return copyFromUuid(fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::Url, std::string_view(name)));
}


CUuid CUuid_createOidUuid(const char* name)
{
    return copyFromUuid(fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::Oid, std::string_view(name)));
}


CUuid CUuid_createX500dnUuid(const char* name)
{
    return copyFromUuid(fuurin::Uuid::createNamespaceUuid(fuurin::Uuid::Ns::X500dn, std::string_view(name)));
}
