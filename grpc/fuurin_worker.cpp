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

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <iostream>
#include <string>
#include <memory>


void runService(const std::string srv_addr)
{
    WorkerServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(srv_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server{builder.BuildAndStart()};
    std::cout << "Service listening on " << srv_addr << std::endl;

    server->Wait();
}


int main(int argc, char** argv)
{
    runService("0.0.0.0:50051");
    return 0;
}
