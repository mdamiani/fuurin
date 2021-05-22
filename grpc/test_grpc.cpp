/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE grpc
#include <boost/test/unit_test.hpp>

#include "fuurin_worker_impl.h"
#include "fuurin_cli_impl.h"
#include "fuurin/broker.h"
#include "fuurin/worker.h"

#include <grpcpp/create_channel.h>

#include <string>
#include <future>
#include <memory>
#include <list>


namespace utf = boost::unit_test;
using namespace std::literals::string_literals;
using namespace std::literals::chrono_literals;


class TestWorkerServiceImpl
{
public:
    static fuurin::Worker* getWorker(WorkerServiceImpl* p)
    {
        return p->worker_.get();
    }
};


struct ServiceFixture
{
    ServiceFixture()
    {
        const std::string address{"localhost:50051"};
        brokFut = broker.start();
        std::tie(service, servFut, servCanc) = WorkerServiceImpl::Run(address);
        client = std::make_unique<WorkerCli>(grpc::CreateChannel(address, grpc::InsecureChannelCredentials()));
        worker = TestWorkerServiceImpl::getWorker(service.get());
    }

    ~ServiceFixture()
    {
        broker.stop();
        brokFut.get();

        servCanc();
        servFut.get();
    }

    fuurin::Broker broker;
    fuurin::Worker* worker;
    std::unique_ptr<WorkerServiceImpl> service;
    std::unique_ptr<WorkerCli> client;
    std::future<void> servFut;
    std::future<void> brokFut;
    WorkerServiceImpl::CancelFn servCanc;
};


fuurin::Uuid fromGrpcUuid(Uuid& uuid)
{
    fuurin::Uuid::Bytes bytes;
    std::fill(bytes.begin(), bytes.end(), 0);

    std::copy_n(uuid.mutable_data()->data(),
        std::min(uuid.mutable_data()->size(), std::tuple_size<fuurin::Uuid::Bytes>::value),
        bytes.data());

    return fuurin::Uuid::fromBytes(bytes);
}


auto waitForGRPCEvents(WorkerCli* client, std::list<Event_Type> sequence, std::chrono::milliseconds timeout)
    -> std::list<Event>
{
    std::list<Event> ret;

    if (sequence.empty())
        return ret;

    auto evf = std::async(std::launch::async, &WorkerCli::WaitForEvent, client, timeout,
        [&ret, &sequence](const Event& ev) {
            auto type = sequence.front();
            if (ev.type() != type)
                return true; // continue

            sequence.pop_front();
            ret.push_back(ev);
            return !sequence.empty();
        });

    BOOST_TEST(evf.get());

    return ret;
}


BOOST_FIXTURE_TEST_CASE(testGetUuid, ServiceFixture)
{
    auto wantUuid = worker->uuid();
    auto gotUuid = client->GetUuid();

    BOOST_TEST(gotUuid.has_value());
    BOOST_TEST(wantUuid == fromGrpcUuid(*gotUuid));
}


BOOST_FIXTURE_TEST_CASE(testGetSeqNum, ServiceFixture)
{
    auto wantSeqn = worker->seqNumber();
    auto gotSeqn = client->GetSeqNum();

    BOOST_TEST(gotSeqn.has_value());
    BOOST_TEST(wantSeqn == gotSeqn->value());
}


BOOST_FIXTURE_TEST_CASE(testRCPSetupTeardown, ServiceFixture)
{
    auto events = waitForGRPCEvents(client.get(), {Event::RCPSetup, Event::RCPTeardown}, 2s);
    BOOST_TEST((events.size() == 2));
}


BOOST_FIXTURE_TEST_CASE(testStart, ServiceFixture)
{
    BOOST_TEST(client->Start());
    auto events = waitForGRPCEvents(client.get(), {Event::Started}, 5s);
    BOOST_TEST((events.size() == 1));
}


BOOST_FIXTURE_TEST_CASE(testSetConfig, ServiceFixture)
{
    BOOST_TEST(client->SetSubscriptions(true, {"topicA", "topicB"}));

    auto [gotAll, gotNames] = worker->topicsNames();

    BOOST_TEST(gotAll == true);
    BOOST_TEST(gotNames.size() == size_t(2));
    BOOST_TEST(gotNames[0] == "topicA"s);
    BOOST_TEST(gotNames[1] == "topicB"s);
}
