/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "cutils.h"

#include <tuple>
#include <algorithm>


using namespace std::literals::string_view_literals;


namespace fuurin {
namespace c {

CUuid uuidConvert(const Uuid& id)
{
    CUuid ret;

    static_assert(std::tuple_size_v<fuurin::Uuid::Bytes> == sizeof(CUuid::bytes));
    std::copy_n(id.bytes().data(), id.size(), ret.bytes);

    return ret;
}


Uuid uuidConvert(const CUuid& id)
{
    Uuid::Bytes bytes;
    static_assert(bytes.size() == sizeof(id.bytes));

    std::copy_n(id.bytes, sizeof(id.bytes), bytes.begin());
    return Uuid::fromBytes(bytes);
}


void logError(std::string_view err) noexcept
{
    log::Arg args[] = {log::Arg{"error"sv, err}};
    log::Logger::error({__FILE__, __LINE__}, args, 1);
}


void logError(const std::exception& e) noexcept
{
    logError(std::string_view(e.what()));
}


void logError(const err::Error& e) noexcept
{
    log::Arg args[] = {log::Arg{"error"sv, std::string_view(e.what())}, e.arg()};
    log::Logger::error(e.loc(), args, 2);
}


void logError() noexcept
{
    try {
        throw;
    } catch (const err::Error& e) {
        logError(e);
    } catch (const std::exception& e) {
        logError(e);
    } catch (...) {
        logError("unknown"sv);
    }
}

} // namespace c
} // namespace fuurin
