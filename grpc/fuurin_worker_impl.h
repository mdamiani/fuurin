/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_WORKER_IMPL_H
#define FUURIN_WORKER_IMPL_H

#include "worker.grpc.pb.h"

#include <grpc/grpc.h>
#include <grpcpp/server_context.h>

#include <memory>


namespace fuurin {
class Worker;
}


class WorkerServiceImpl final : public WorkerService::Service
{
public:
    explicit WorkerServiceImpl();
    ~WorkerServiceImpl();

    grpc::Status GetUuid(grpc::ServerContext*,
        const google::protobuf::Empty*,
        Uuid* response) override;

    grpc::Status GetSeqNum(grpc::ServerContext*,
        const google::protobuf::Empty*,
        SeqNum* response) override;


private:
    std::unique_ptr<fuurin::Worker> worker_;
};

#endif // FUURIN_WORKER_IMPL_H
