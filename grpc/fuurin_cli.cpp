/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "worker.grpc.pb.h"

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <iostream>
#include <memory>
#include <optional>


class WorkerCli final
{
public:
    WorkerCli(std::shared_ptr<grpc::Channel> channel)
        : stub_(WorkerService::NewStub(channel))
    {
    }

    std::optional<SeqNum> GetSeqNum()
    {
        grpc::ClientContext context;
        SeqNum ret;

        const grpc::Status status = stub_->GetSeqNum(&context, google::protobuf::Empty{}, &ret);
        if (!status.ok())
            return {};

        return {ret};
    }

private:
    std::unique_ptr<WorkerService::Stub> stub_;
};


int main(int argc, char** argv)
{
    WorkerCli cli{grpc::CreateChannel("localhost:50051",
        grpc::InsecureChannelCredentials())};

    const auto seqNum = cli.GetSeqNum();
    std::cout << "SeqNum: " << (seqNum ? std::to_string(seqNum->value()) : "n/a") << std::endl;

    return 0;
}
