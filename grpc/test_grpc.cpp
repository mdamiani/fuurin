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
#include <boost/test/data/test_case.hpp>

#include "fuurin_worker_impl.h"
#include "fuurin_cli_impl.h"
#include "fuurin/broker.h"
#include "fuurin/worker.h"

#include <google/protobuf/util/message_differencer.h>
#include <grpcpp/create_channel.h>

#include <string>
#include <future>
#include <memory>
#include <list>


namespace utf = boost::unit_test;
namespace bdata = utf::data;

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


auto waitForGRPCEvents(
    WorkerCli* client,
    std::chrono::milliseconds timeout,
    std::list<Event_Type> sequence,
    std::list<Event_Type> evCallType = {Event::RCPSetup},
    std::function<void(Event_Type type)> evCallFn = [](Event_Type) {})
    -> std::list<Event>
{
    std::list<Event> ret;

    if (sequence.empty())
        return ret;

    auto succ = client->WaitForEvent(timeout,
        [&ret, &sequence, evCallType, evCallFn](const Event& ev) {
            for (auto callType : evCallType) {
                if (ev.type() == callType) {
                    evCallFn(callType);
                    break;
                }
            }

            auto type = sequence.front();

            if (ev.type() != type)
                return true; // continue

            sequence.pop_front();
            ret.push_back(ev);
            return !sequence.empty();
        });

    BOOST_TEST(succ);

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


BOOST_FIXTURE_TEST_CASE(testEventsTimeout, ServiceFixture)
{
    auto events = waitForGRPCEvents(client.get(), 2s, {Event::Started});
    BOOST_TEST((events.empty()));
}


BOOST_FIXTURE_TEST_CASE(testRCPSetupTeardown, ServiceFixture)
{
    auto events = waitForGRPCEvents(client.get(), 2s, {Event::RCPSetup, Event::RCPTeardown});
    BOOST_TEST((events.size() == 2));
}


BOOST_FIXTURE_TEST_CASE(testStart, ServiceFixture)
{
    auto events = waitForGRPCEvents(client.get(), 5s,
        {Event::Started},
        {Event::RCPSetup},
        [this](Event_Type) {
            BOOST_TEST(client->Start());
        });
    BOOST_TEST((events.size() == 1));
}


BOOST_FIXTURE_TEST_CASE(testStop, ServiceFixture)
{
    auto events = waitForGRPCEvents(client.get(), 5s,
        {Event::Stopped},
        {Event::RCPSetup},
        [this](Event_Type) {
            BOOST_TEST(client->Start());
            BOOST_TEST(client->Stop());
        });
    BOOST_TEST((events.size() == 1));
}


BOOST_FIXTURE_TEST_CASE(testOnline, ServiceFixture)
{
    auto events = waitForGRPCEvents(client.get(), 5s,
        {Event::Online},
        {Event::RCPSetup},
        [this](Event_Type) {
            BOOST_TEST(client->Start());
        });
    BOOST_TEST((events.size() == 1));
}


BOOST_FIXTURE_TEST_CASE(testOffline, ServiceFixture)
{
    auto events = waitForGRPCEvents(client.get(), 5s,
        {Event::Offline},
        {Event::RCPSetup, Event::Online},
        [this](Event_Type type) {
            if (type == Event::RCPSetup)
                BOOST_TEST(client->Start());

            if (type == Event::Online)
                BOOST_TEST(client->Stop());
        });
    BOOST_TEST((events.size() == 1));
}


BOOST_FIXTURE_TEST_CASE(testSetConfigTopicsNames, ServiceFixture)
{
    BOOST_TEST(client->SetSubscriptions(false, {"topicA", "topicB"}));

    auto events = waitForGRPCEvents(client.get(), 5s,
        {Event::Started},
        {Event::RCPSetup},
        [this](Event_Type) {
            BOOST_TEST(client->Start());
        });

    BOOST_REQUIRE((events.size() == 1));
    auto ev = events.front();
    auto ss = ev.mutable_configevent()->mutable_config()->mutable_subscriptions();

    BOOST_TEST(ss->wildcard() == false);
    BOOST_TEST(ss->name_size() == 2);
    BOOST_TEST(ss->name(0) == "topicA"s);
    BOOST_TEST(ss->name(1) == "topicB"s);
}


BOOST_FIXTURE_TEST_CASE(testSetConfigTopicsAll, ServiceFixture)
{
    BOOST_TEST(client->SetSubscriptions(true, {}));

    auto events = waitForGRPCEvents(client.get(), 5s,
        {Event::Started},
        {Event::RCPSetup},
        [this](Event_Type) {
            BOOST_TEST(client->Start());
        });

    BOOST_TEST((events.size() == 1));
    auto ev = events.front();
    auto ss = ev.mutable_configevent()->mutable_config()->mutable_subscriptions();

    BOOST_TEST(ss->wildcard() == true);
    BOOST_TEST(ss->name_size() == 0);
}


BOOST_FIXTURE_TEST_CASE(testDispatchSingle, ServiceFixture)
{
    auto events = waitForGRPCEvents(client.get(), 5s,
        {Event::Delivery, Event::Delivery},
        {Event::RCPSetup, Event::Online},
        [this](Event_Type type) {
            if (type == Event::RCPSetup)
                BOOST_TEST(client->Start());

            if (type == Event::Online)
                BOOST_TEST(client->Dispatch({{"topicA", "Hello"}, {"topicB", "World"}}));
        });

    BOOST_REQUIRE((events.size() == 2));

    uint64_t wantSeqn = 1;
    for (auto& ev : events) {
        auto te = ev.mutable_topicevent();

        BOOST_TEST(broker.uuid() == fromGrpcUuid(*te->mutable_broker()));
        BOOST_TEST(worker->uuid() == fromGrpcUuid(*te->mutable_worker()));
        BOOST_TEST(wantSeqn == te->mutable_seqn()->value());

        auto tt = te->mutable_topic();

        if (wantSeqn == 1) {
            BOOST_TEST(tt->type() == Topic_Type_State);
            BOOST_TEST(tt->name() == "topicA"s);
            BOOST_TEST(tt->data() == "Hello"s);
        } else if (wantSeqn == 2) {
            BOOST_TEST(tt->type() == Topic_Type_State);
            BOOST_TEST(tt->name() == "topicB"s);
            BOOST_TEST(tt->data() == "World"s);
        }

        ++wantSeqn;
    }

    BOOST_TEST(worker->seqNumber() == 2ull);
}


BOOST_DATA_TEST_CASE_F(ServiceFixture, testDispatchConcurrent,
    bdata::make({
        Topic_Type_State,
        Topic_Type_Event,
    }),
    evType)
{
    auto promise = std::promise<void>{};
    auto setup = promise.get_future();

    auto f1 = std::async(std::launch::async, [this, &promise]() {
        return waitForGRPCEvents(client.get(), 5s,
            {Event::Delivery, Event::Delivery},
            {Event::RCPSetup, Event::Online},
            [this, &promise](Event_Type type) {
                if (type == Event::RCPSetup)
                    BOOST_TEST(client->Start());

                if (type == Event::Online)
                    promise.set_value();
            });
    });

    setup.get();

    const auto sendTopic = [this, evType]() {
        BOOST_TEST(client->Dispatch({{"topicA", "Hello"}}, evType));
    };

    auto f2 = std::async(std::launch::async, sendTopic);
    auto f3 = std::async(std::launch::async, sendTopic);

    f3.get();
    f2.get();

    auto ev = f1.get();

    BOOST_REQUIRE((ev.size() == 2));

    auto t1 = ev.front().mutable_topicevent();
    auto t2 = ev.back().mutable_topicevent();

    BOOST_TEST(t1->seqn().value() + 1 == t2->seqn().value());
    BOOST_TEST(t1->broker().data() == t2->broker().data());
    BOOST_TEST(t1->worker().data() == t2->worker().data());
    BOOST_TEST(t1->mutable_topic()->type() == t2->mutable_topic()->type());
    BOOST_TEST(t1->mutable_topic()->name() == t2->mutable_topic()->name());
    BOOST_TEST(t1->mutable_topic()->data() == t2->mutable_topic()->data());
}


BOOST_FIXTURE_TEST_CASE(testEventsMultiplex, ServiceFixture)
{
    auto promise = std::promise<void>{};
    auto setup = promise.get_future();

    auto f1 = std::async(std::launch::async, [this, &promise]() {
        return waitForGRPCEvents(client.get(), 5s,
            {Event::Delivery, Event::Delivery},
            {Event::RCPSetup},
            [&promise](Event_Type) {
                promise.set_value();
            });
    });

    setup.get();

    auto f2 = std::async(std::launch::async, [this]() {
        return waitForGRPCEvents(client.get(), 5s,
            {Event::Delivery, Event::Delivery},
            {Event::RCPSetup, Event::Online},
            [this](Event_Type type) {
                if (type == Event::RCPSetup)
                    BOOST_TEST(client->Start());

                if (type == Event::Online)
                    BOOST_TEST(client->Dispatch({{"topicA", "Hello"}, {"topicB", "World"}}));
            });
    });

    auto events1 = f1.get();
    auto events2 = f2.get();

    BOOST_REQUIRE((events1.size() == 2));
    BOOST_REQUIRE((events2.size() == 2));

    for (auto it1 = events1.begin(), it2 = events2.begin();
         it1 != events1.end() && it2 != events2.end();
         ++it1, ++it2) //
    {
        BOOST_TEST(google::protobuf::util::MessageDifferencer::Equals(*it1, *it2));
    }
}


BOOST_FIXTURE_TEST_CASE(testSyncOk, ServiceFixture)
{
    // store topics
    waitForGRPCEvents(client.get(), 5s,
        {Event::Delivery, Event::Delivery},
        {Event::RCPSetup, Event::Online},
        [this](Event_Type type) {
            if (type == Event::RCPSetup)
                BOOST_TEST(client->Start());

            if (type == Event::Online)
                BOOST_TEST(client->Dispatch({{"topicA", "Hello"}, {"topicB", "World"}}));
        });

    std::list<Event_Type> filters{
        Event::SyncDownloadOn,
        Event::SyncRequest,
        Event::SyncBegin,
        Event::SyncElement,
        Event::SyncElement,
        Event::SyncSuccess,
        Event::SyncDownloadOff,
    };

    // sync topics
    auto events = waitForGRPCEvents(client.get(), 5s,
        filters, {Event::RCPSetup},
        [this](Event_Type) {
            BOOST_TEST(client->Sync());
        });

    BOOST_TEST(events.size() == filters.size());

    uint64_t wantSeqn{0};

    for (auto& ev : events) {
        BOOST_REQUIRE(!filters.empty());
        const auto type = filters.front();
        filters.pop_front();

        BOOST_REQUIRE(ev.type() == type);

        switch (type) {
        case Event::SyncRequest: {
            auto cfgev = ev.mutable_configevent();
            BOOST_TEST(fromGrpcUuid(*cfgev->mutable_uuid()) == worker->uuid());
            BOOST_TEST(cfgev->mutable_seqn()->value() == 2ull);

            auto cfgsub = cfgev->mutable_config()->mutable_subscriptions();
            BOOST_TEST(cfgsub->wildcard() == true);
            BOOST_TEST(cfgsub->name_size() == 0);
            break;
        }

        case Event::SyncElement: {
            auto tt = ev.mutable_topicevent();
            BOOST_TEST(tt->seqn().value() == ++wantSeqn);
            BOOST_TEST(fromGrpcUuid(*tt->mutable_broker()) == broker.uuid());
            BOOST_TEST(fromGrpcUuid(*tt->mutable_worker()) == worker->uuid());
            BOOST_TEST(tt->mutable_topic()->type() == Topic_Type_State);
            if (wantSeqn == 1) {
                BOOST_TEST(tt->mutable_topic()->name() == "topicA"s);
                BOOST_TEST(tt->mutable_topic()->data() == "Hello"s);
            } else {
                BOOST_TEST(tt->mutable_topic()->name() == "topicB"s);
                BOOST_TEST(tt->mutable_topic()->data() == "World"s);
            }
            break;
        }

        default:
            break;
        }
    }
}


BOOST_FIXTURE_TEST_CASE(testSyncErr, ServiceFixture)
{
    // stop broker
    broker.stop();

    std::list<Event_Type> filters{
        Event::SyncDownloadOn,
        Event::SyncRequest,
        Event::SyncRequest,
        Event::SyncError,
        Event::SyncDownloadOff,
    };

    // sync topics
    auto events = waitForGRPCEvents(client.get(), 15s,
        filters, {Event::RCPSetup},
        [this](Event_Type) {
            BOOST_TEST(client->Start());
            BOOST_TEST(client->Sync());
        });

    BOOST_TEST(events.size() == filters.size());

    for (auto& ev : events) {
        BOOST_REQUIRE(!filters.empty());
        const auto type = filters.front();
        filters.pop_front();

        BOOST_REQUIRE(ev.type() == type);
    }
}


BOOST_AUTO_TEST_CASE(shutdownWaitForEventServer, *utf::timeout(10))
{
    const std::string address{"localhost:50051"};
    auto [service, servFut, servCanc] = WorkerServiceImpl::Run(address);
    auto client = WorkerCli{grpc::CreateChannel(address, grpc::InsecureChannelCredentials())};

    auto f = std::async(std::launch::async, [&client]() {
        return client.WaitForEvent(0s, [](Event) { return true; });
    });

    servCanc();
    servFut.get();

    BOOST_TEST(!f.get());
}


BOOST_AUTO_TEST_CASE(shutdownWaitForEventClient, *utf::timeout(10))
{
    const std::string address{"localhost:50051"};
    auto [service, servFut, servCanc] = WorkerServiceImpl::Run(address);
    auto client = WorkerCli{grpc::CreateChannel(address, grpc::InsecureChannelCredentials())};

    BOOST_TEST(client.WaitForEvent(0s, [](Event) { return false; }));

    servCanc();
    servFut.get();
}
