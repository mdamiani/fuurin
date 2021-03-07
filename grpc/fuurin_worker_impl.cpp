/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin_worker_impl.h"

#include "fuurin/worker.h"
#include "fuurin/uuid.h"


WorkerServiceImpl::WorkerServiceImpl()
    : worker_{new fuurin::Worker}
{
}


WorkerServiceImpl::~WorkerServiceImpl()
{
}


grpc::Status WorkerServiceImpl::GetUuid(grpc::ServerContext*,
    const google::protobuf::Empty*, Uuid* resp)
{
    const auto bytes = worker_->uuid().bytes();
    resp->set_data(bytes.data(), bytes.size());
    return grpc::Status::OK;
}


grpc::Status WorkerServiceImpl::GetSeqNum(grpc::ServerContext*,
    const google::protobuf::Empty*, SeqNum* resp)
{
    resp->set_value(worker_->seqNumber());
    return grpc::Status::OK;
}
