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

#include <grpcpp/create_channel.h>

#include <iostream>


int main(int argc, char** argv)
{
    WorkerCli cli{grpc::CreateChannel("localhost:50051",
        grpc::InsecureChannelCredentials())};

    const auto seqNum = cli.GetSeqNum();
    std::cout << "SeqNum: " << (seqNum ? std::to_string(seqNum->value()) : "n/a") << std::endl;

    return 0;
}
