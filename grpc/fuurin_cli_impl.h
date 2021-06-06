/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_CLI_IMPL_H
#define FUURIN_CLI_IMPL_H


#include "worker.grpc.pb.h"

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>

#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <utility>
#include <chrono>


class WorkerCli final
{
public:
    WorkerCli(std::shared_ptr<grpc::Channel> channel);

    std::optional<Uuid> GetUuid();
    std::optional<SeqNum> GetSeqNum();

    bool SetEndpoints(std::vector<std::string> delivery, std::vector<std::string> dispatch,
        std::vector<std::string> snapshot);
    bool SetSubscriptions(bool wildcard, std::vector<std::string> names);

    bool Start();
    bool Stop();
    bool Sync();

    bool Dispatch(const std::vector<std::pair<std::string, std::string>>& stream,
        Topic_Type type = Topic_Type_State);

    bool WaitForEvent(std::chrono::milliseconds timeout, std::function<bool(Event)> callback);

private:
    std::unique_ptr<WorkerService::Stub> stub_;
};

#endif // FUURIN_CLI_IMPL_H
