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


WorkerServiceImpl::WorkerServiceImpl()
{
}

grpc::Status WorkerServiceImpl::GetSeqNum(grpc::ServerContext* context,
    const google::protobuf::Empty* request, SeqNum* response)
{
    response->set_value(1);
    return grpc::Status::OK;
}
