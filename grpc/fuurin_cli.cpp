/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin_cli_impl.h"

#include "fuurin/uuid.h"

#include <grpcpp/create_channel.h>

#include <algorithm>
#include <iostream>
#include <future>
#include <functional>

using namespace std::literals::chrono_literals;


fuurin::Uuid fromGrpcUuid(Uuid& uuid)
{
    fuurin::Uuid::Bytes bytes;
    std::fill(bytes.begin(), bytes.end(), 0);

    std::copy_n(uuid.mutable_data()->data(),
        std::min(uuid.mutable_data()->size(), std::tuple_size<fuurin::Uuid::Bytes>::value),
        bytes.data());

    return fuurin::Uuid::fromBytes(bytes);
}


int main(int, char**)
{
    WorkerCli cli{grpc::CreateChannel("localhost:50051",
        grpc::InsecureChannelCredentials())};

    auto uuid = cli.GetUuid();
    auto seqNum = cli.GetSeqNum();

    auto callback = std::bind(&WorkerCli::WaitForEvent, &cli, 3s, [](const Event& ev) {
        std::cout
            << "Event:\n"
            << ev.DebugString()
            << std::endl;
    });

    auto evf = std::async(std::launch::async, callback);

    std::cout
        << "Uuid: "
        << (uuid ? fromGrpcUuid(*uuid).toShortString() : "n/a")
        << std::endl;

    std::cout
        << "SeqNum: "
        << (seqNum ? std::to_string(seqNum->value()) : "n/a")
        << std::endl;

    std::cout
        << "Subscribe: topicA, topicB: "
        << (cli.SetSubscriptions(false, {"topicA", "topicB"}) ? "ok" : "err")
        << std::endl;

    std::cout
        << "Start: "
        << (cli.Start() ? "ok" : "err")
        << std::endl;

    std::cout
        << "Sync: "
        << (cli.Sync() ? "ok" : "err")
        << std::endl;

    std::cout
        << "Dispatch: "
        << (cli.Dispatch({{"topicA", "Hello"}, {"topicB", "World"}}) ? "ok" : "err")
        << std::endl;

    evf.get();

    return 0;
}
