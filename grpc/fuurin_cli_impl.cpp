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


WorkerCli::WorkerCli(std::shared_ptr<grpc::Channel> channel)
    : stub_(WorkerService::NewStub(channel))
{
}

std::optional<SeqNum> WorkerCli::GetSeqNum()
{
    grpc::ClientContext context;
    SeqNum ret;

    const grpc::Status status = stub_->GetSeqNum(&context, google::protobuf::Empty{}, &ret);
    if (!status.ok())
        return {};

    return {ret};
}
