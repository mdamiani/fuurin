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
#include <string>
#include <future>
#include <type_traits>
#include <functional>
#include <tuple>
#include <atomic>


namespace fuurin {
namespace zmq {
class Socket;
class Part;
class Cancellation;
} // namespace zmq

class Worker;
} // namespace fuurin


class WorkerServiceImpl final : public WorkerService::Service
{
public:
    enum struct RPC : uint8_t
    {
        Cancel,
        GetUuid,
        GetSeqNum,
        SetConfig,
        SetStart,
        SetStop,
        SetSync,
        SetDispatch,
    };

    using RPCType = std::underlying_type_t<RPC>;
    using CancelFn = std::function<void()>;

    /**
     * Disable copy.
     */
    ///@{
    WorkerServiceImpl(const WorkerServiceImpl&) = delete;
    WorkerServiceImpl& operator=(const WorkerServiceImpl&) = delete;
    ///@}


public:
    static auto Run(const std::string& addr) -> std::tuple<
        std::unique_ptr<WorkerServiceImpl>,
        std::future<void>,
        CancelFn>;

    ~WorkerServiceImpl() noexcept;


protected:
    grpc::Status GetUuid(grpc::ServerContext*,
        const google::protobuf::Empty*,
        Uuid* response) override;

    grpc::Status GetSeqNum(grpc::ServerContext*,
        const google::protobuf::Empty*,
        SeqNum* response) override;

    grpc::Status SetConfig(grpc::ServerContext*,
        const Config* conf,
        google::protobuf::Empty*) override;

    grpc::Status Start(grpc::ServerContext*,
        const google::protobuf::Empty*,
        google::protobuf::Empty*) override;

    grpc::Status Stop(grpc::ServerContext*,
        const google::protobuf::Empty*,
        google::protobuf::Empty*) override;

    grpc::Status Sync(grpc::ServerContext*,
        const google::protobuf::Empty*,
        google::protobuf::Empty*) override;

    grpc::Status Dispatch(grpc::ServerContext*,
        grpc::ServerReader<Topic>* stream,
        google::protobuf::Empty*) override;

    grpc::Status WaitForEvent(grpc::ServerContext*,
        const EventTimeout* timeout,
        grpc::ServerWriter<Event>* stream) override;


private:
    explicit WorkerServiceImpl(const std::string& server_addr);

    void runServer(std::promise<void>* started);
    void runClient();
    void runEvents();

    void shutdown();

    void sendRPC(RPC type);
    void sendRPC(RPC type, fuurin::zmq::Part&& pay);
    fuurin::zmq::Part recvRPC();
    fuurin::zmq::Part serveRPC(RPC type, const fuurin::zmq::Part& pay);

    void setConfig(const Config& conf);
    void setDispatch(const Topic& topic);
    std::optional<Event> getEvent(const fuurin::zmq::Part& pay) const;


private:
    friend class TestWorkerServiceImpl;

    const std::string server_addr_;
    const std::unique_ptr<fuurin::Worker> worker_;
    const std::unique_ptr<fuurin::zmq::Socket> zrpcClient_;
    const std::unique_ptr<fuurin::zmq::Socket> zrpcServer_;
    const std::unique_ptr<fuurin::zmq::Socket> zrpcEvents_;
    const std::unique_ptr<fuurin::zmq::Cancellation> zcanc1_;
    const std::unique_ptr<fuurin::zmq::Cancellation> zcanc2_;

    std::atomic<uint32_t> cancNum_;

    std::future<void> client_;
    std::future<void> events_;
    std::future<void> active_;

    std::unique_ptr<grpc::Server> server_;
};

#endif // FUURIN_WORKER_IMPL_H
