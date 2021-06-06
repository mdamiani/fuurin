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


std::optional<Uuid> WorkerCli::GetUuid()
{
    grpc::ClientContext context;
    Uuid ret;

    const grpc::Status status = stub_->GetUuid(&context, google::protobuf::Empty{}, &ret);
    if (!status.ok())
        return {};

    return {ret};
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


bool WorkerCli::SetEndpoints(std::vector<std::string> delivery, std::vector<std::string> dispatch,
    std::vector<std::string> snapshot)
{
    grpc::ClientContext context;
    Endpoints endp;

    for (const auto& el : delivery) {
        endp.add_delivery(el);
    }
    for (const auto& el : dispatch) {
        endp.add_dispatch(el);
    }
    for (const auto& el : snapshot) {
        endp.add_snapshot(el);
    }

    google::protobuf::Empty ret;
    const grpc::Status status = stub_->SetEndpoints(&context, endp, &ret);

    return status.ok();
}


bool WorkerCli::SetSubscriptions(bool wildcard, std::vector<std::string> names)
{
    grpc::ClientContext context;
    Subscriptions subscr;

    subscr.set_wildcard(wildcard);
    for (const auto& name : names)
        subscr.add_name(name);

    google::protobuf::Empty ret;
    const grpc::Status status = stub_->SetSubscriptions(&context, subscr, &ret);

    return status.ok();
}


bool WorkerCli::Start()
{
    grpc::ClientContext context;

    google::protobuf::Empty ret;
    const grpc::Status status = stub_->Start(&context, google::protobuf::Empty{}, &ret);

    return status.ok();
}


bool WorkerCli::Stop()
{
    grpc::ClientContext context;

    google::protobuf::Empty ret;
    const grpc::Status status = stub_->Stop(&context, google::protobuf::Empty{}, &ret);

    return status.ok();
}


bool WorkerCli::Sync()
{
    grpc::ClientContext context;

    google::protobuf::Empty ret;
    const grpc::Status status = stub_->Sync(&context, google::protobuf::Empty{}, &ret);

    return status.ok();
}


bool WorkerCli::Dispatch(const std::vector<std::pair<std::string, std::string>>& stream, Topic_Type type)
{
    grpc::ClientContext context;

    google::protobuf::Empty ret;
    std::unique_ptr<grpc::ClientWriter<Topic>> writer(stub_->Dispatch(&context, &ret));

    for (const auto& el : stream) {
        Topic t;
        t.set_name(el.first);
        t.set_data(el.second);
        t.set_type(type);

        if (!writer->Write(t))
            break;
    }

    writer->WritesDone();
    grpc::Status status = writer->Finish();

    return status.ok();
}


bool WorkerCli::WaitForEvent(std::chrono::milliseconds timeout, std::function<bool(Event)> callback)
{
    grpc::ClientContext context;

    EventTimeout req;
    req.set_millis(timeout.count());

    std::unique_ptr<grpc::ClientReader<Event>> reader(stub_->WaitForEvent(&context, req));

    Event ev;
    bool cancelled = false;

    while (reader->Read(&ev)) {
        if (!callback(ev)) {
            context.TryCancel();
            cancelled = true;
            break;
        }
    }

    grpc::Status status = reader->Finish();

    return status.ok() || cancelled;
}
