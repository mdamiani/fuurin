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
#include "utils.h"

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_context.h>

#include <memory>
#include <string>
#include <future>
#include <type_traits>
#include <functional>
#include <tuple>
#include <atomic>
#include <chrono>
#include <optional>


namespace fuurin {
namespace zmq {
class Socket;
class Part;
class Cancellation;
} // namespace zmq

class Worker;
} // namespace fuurin


/**
 * \brief GRPC server implementation.
 */
class WorkerServiceImpl final : public WorkerService::Service
{
public:
    ///< Cancel function type, for server stopping.
    using CancelFn = std::function<void()>;

    ///< Latency for other monitoring events.
    static constexpr std::chrono::seconds LatencyDuration{5};


    /**
     * Disable copy.
     */
    ///@{
    WorkerServiceImpl(const WorkerServiceImpl&) = delete;
    WorkerServiceImpl& operator=(const WorkerServiceImpl&) = delete;
    ///@}


public:
    /**
     * \brief Runs the GRPC server.
     *
     * \param[in] addr Server address to listen to.
     * \param[in] endp Connection endpoints.
     *
     * \return A tuple with a server instantion, a future to wait for
     *      and a function to stop the server.
     */
    static auto Run(const std::string& addr, const utils::Endpoints& endp) -> std::tuple<
        std::unique_ptr<WorkerServiceImpl>,
        std::future<void>,
        CancelFn,
        utils::Endpoints,
        bool>;

    /**
     * \brief Destructor.
     */
    ~WorkerServiceImpl() noexcept;


protected:
    /**
     * \brief Type of requested RPC.
     */
    enum struct RPC : uint8_t
    {
        Cancel,           ///< Stop server.
        GetUuid,          ///< Get UUID.
        GetSeqNum,        ///< Get sequence number.
        SetEndpoints,     ///< Set connection endpoints.
        SetSubscriptions, ///< Set topics subscriptions.
        SetStart,         ///< Start worker and connect to the broker.
        SetStop,          ///< Stop worker and disconnect from broker.
        SetSync,          ///< Synchronize with broker.
        SetDispatch,      ///< Dispatches data to the broker.
    };

    ///< Underlying type of RPC kind.
    using RPCType = std::underlying_type_t<RPC>;

    /**
     * \brief RPC to get worker UUID.
     */
    grpc::Status GetUuid(grpc::ServerContext*,
        const google::protobuf::Empty*,
        Uuid* response) override;

    /**
     * \brief RPC to get worker sequence number.
     */
    grpc::Status GetSeqNum(grpc::ServerContext*,
        const google::protobuf::Empty*,
        SeqNum* response) override;

    /**
     * \brief RPC to set worker endpoints.
     */
    grpc::Status SetEndpoints(grpc::ServerContext*,
        const Endpoints* endp,
        google::protobuf::Empty*) override;

    /**
     * \brief RPC to set worker configuration.
     */
    grpc::Status SetSubscriptions(grpc::ServerContext*,
        const Subscriptions* subscr,
        google::protobuf::Empty*) override;

    /**
     * \brief RPC to start worker and connection with broker.
     */
    grpc::Status Start(grpc::ServerContext*,
        const google::protobuf::Empty*,
        google::protobuf::Empty*) override;

    /**
     * \brief RPC to stop worker and disconnection from broker.
     */
    grpc::Status Stop(grpc::ServerContext*,
        const google::protobuf::Empty*,
        google::protobuf::Empty*) override;

    /**
     * \brief RPC to sync worker with broker.
     */
    grpc::Status Sync(grpc::ServerContext*,
        const google::protobuf::Empty*,
        google::protobuf::Empty*) override;

    /**
     * \brief RPC to dispatch worker topic to broker.
     */
    grpc::Status Dispatch(grpc::ServerContext*,
        grpc::ServerReader<Topic>* stream,
        google::protobuf::Empty*) override;

    /**
     * \brief RPC to wait for worker/broker events.
     */
    grpc::Status WaitForEvent(grpc::ServerContext*,
        const EventTimeout* timeout,
        grpc::ServerWriter<Event>* stream) override;


private:
    /**
     * \brief Constructor.
     * \param[in] server_addr Server address.
     */
    explicit WorkerServiceImpl(const std::string& server_addr);

    /**
     * \brief Runs the GRPC main loop and waits for clients requests.
     *
     * \param[in] started Promise to sets when the servers has actually created and started.
     */
    void runServer(std::promise<bool>* started);

    /**
     * \brief Runs the worker main loop, to multiplex client's requests.
     */
    void runClient();

    /**
     * \brief Runs the worker main loop, to read and forwards events.
     */
    void runEvents();

    /**
     * \brief Shutdown the RPC server and stops the worker.
     */
    void shutdown();

    /**
     * \brief Sends an RPC request to the worker main loop.
     *
     * \param[in] type Type of RPC.
     */
    ///@{
    void sendRPC(RPC type);
    void sendRPC(RPC type, fuurin::zmq::Part&& pay);
    ///}@

    /**
     * \brief Receives an RPC request from the worker main loop.
     */
    fuurin::zmq::Part recvRPC();

    /**
     * \brief Servers an RPC rquest, from the main worker loop.
     *
     * \param[in] type RCP type.
     * \param[in] pay RCP payload.
     */
    fuurin::zmq::Part serveRPC(RPC type, const fuurin::zmq::Part& pay);

    /**
     * \brief Applies subscriptions to the worker.
     *
     * \param[in] subscr Requested subscriptions.
     */
    void applySubscriptions(const Subscriptions& subscr);

    /**
     * \brief Applies endpoints to the worker.
     *
     * \param[in] endp Requested endpoints.
     */
    void applyEndpoints(const Endpoints& endp);

    /**
     * \brief Actually dispatches a topic with worker.
     *
     * \param[in] topic Requested topic to dispatch.
     */
    void setDispatch(const Topic& topic);

    /**
     * \brief Read an event sent by the main worker events loop.
     *
     * \param[in] pay Payload read from the ZMQ socket.
     *
     * \return A RCP event to send back to the RPC client, in case of no errors.
     */
    std::optional<Event> getEvent(const fuurin::zmq::Part& pay) const;


private:
    ///< Test class.
    friend class TestWorkerServiceImpl;

    const std::string server_addr_;                           ///< Server address to listen to.
    const std::unique_ptr<fuurin::Worker> worker_;            ///< Fuurin worker.
    const std::unique_ptr<fuurin::zmq::Socket> zrpcClient_;   ///< ZMQ socket to send data to worker.
    const std::unique_ptr<fuurin::zmq::Socket> zrpcServer_;   ///< ZMQ socket to read data for worker.
    const std::unique_ptr<fuurin::zmq::Socket> zrpcEvents_;   ///< ZMQ socket to send worker events.
    const std::unique_ptr<fuurin::zmq::Cancellation> zcanc1_; ///< Cancellation for worker loop.
    const std::unique_ptr<fuurin::zmq::Cancellation> zcanc2_; ///< Cancellation for events loop.

    std::atomic<uint32_t> cancNum_; ///< Value to handle multiple WaitForEvent connections.

    std::future<void> client_; ///< Future for worker main loop.
    std::future<void> events_; ///< Future for events main loop.
    std::future<void> active_; ///< Future for worker started.

    std::unique_ptr<grpc::Server> server_; ///< RPC server.
};

#endif // FUURIN_WORKER_IMPL_H
