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


class WorkerServiceImpl final : public WorkerService::Service
{
public:
    explicit WorkerServiceImpl();

    grpc::Status GetSeqNum(grpc::ServerContext* context,
        const google::protobuf::Empty* request,
        SeqNum* response) override;
};

#endif // FUURIN_WORKER_IMPL_H
