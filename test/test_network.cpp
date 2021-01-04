/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE network
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include "test_utils.hpp"

#include "fuurin/broker.h"
#include "fuurin/worker.h"
#include "fuurin/topic.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpoller.h"

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <string_view>


using namespace fuurin;
namespace utf = boost::unit_test;
namespace bdata = utf::data;

using namespace std::literals::string_view_literals;


class Forwarder
{
public:
    Forwarder()
        : zctx_{std::make_unique<zmq::Context>()}
        , stopSnd_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR)}
        , stopRcv_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR)}
        , sockFrom_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::DISH)}
        , sockTo_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::RADIO)}
        , enabled_{true}
    {
        stopSnd_->setEndpoints({"inproc://forwarder-stop"});
        stopRcv_->setEndpoints({"inproc://forwarder-stop"});

        stopRcv_->bind();
        stopSnd_->connect();

        // works for SessionEnv::BrokerHugz, SessionEnv::BrokerUpdt as well.
        sockFrom_->setGroups({SessionEnv::WorkerHugz.data(), SessionEnv::WorkerUpdt.data()});
    }

    enum OpenType
    {
        Bind,
        Connect,
    };

    void setEndpoints(OpenType opnFrom, const std::string& addrFrom, OpenType opnTo, const std::string& addrTo)
    {
        typeFrom_ = opnFrom;
        sockFrom_->setEndpoints({addrFrom});

        typeTo_ = opnTo;
        sockTo_->setEndpoints({addrTo});
    }

    std::future<void> start()
    {
        return std::async(std::launch::async, &Forwarder::run, this);
    }

    void stop()
    {
        stopSnd_->send(zmq::Part{uint8_t(1)});
    }

    void run()
    {
        openSock(sockFrom_.get(), typeFrom_);
        openSock(sockTo_.get(), typeTo_);

        zmq::Poller poll{zmq::PollerEvents::Type::Read, sockFrom_.get(), stopRcv_.get()};

        for (;;) {
            for (auto s : poll.wait()) {
                if (s == sockFrom_.get()) {
                    zmq::Part p;
                    sockFrom_->recv(&p);
                    if (enabled_)
                        sockTo_->send(p);

                } else if (s == stopRcv_.get()) {
                    zmq::Part p;
                    stopRcv_->recv(&p);
                    return;
                }
            }
        }
    }

    void setEnabled(bool v)
    {
        enabled_ = v;
    }


private:
    void openSock(zmq::Socket* sock, OpenType openType)
    {
        switch (openType) {
        case Bind:
            sock->bind();
            break;

        case Connect:
            sock->connect();
            break;
        };
    }


private:
    const std::unique_ptr<zmq::Context> zctx_;
    const std::unique_ptr<zmq::Socket> stopSnd_;
    const std::unique_ptr<zmq::Socket> stopRcv_;
    const std::unique_ptr<zmq::Socket> sockFrom_;
    const std::unique_ptr<zmq::Socket> sockTo_;

    OpenType typeFrom_;
    OpenType typeTo_;
    bool enabled_;
};


struct DispatchFixture
{
    DispatchFixture()
        : w{Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker.net"sv)}
        , b{Uuid::createNamespaceUuid(Uuid::Ns::Dns, "broker.net"sv)}
    {
        const std::vector<std::string> delivery{"ipc:///tmp/worker_delivery"};
        const std::vector<std::string> snapshot{"ipc:///tmp/broker_snapshot"};
        const std::vector<std::string> dispatch{"ipc:///tmp/dispatch_b1", "ipc:///tmp/dispatch_b2"};
        const std::vector<std::string> forwards{"ipc:///tmp/dispatch_f1", "ipc:///tmp/dispatch_f2"};

        b.setEndpoints(delivery, dispatch, snapshot);
        w.setEndpoints(delivery, forwards, snapshot);
        f1.setEndpoints(Forwarder::Bind, forwards.at(0), Forwarder::Connect, dispatch.at(0));
        f2.setEndpoints(Forwarder::Bind, forwards.at(1), Forwarder::Connect, dispatch.at(1));

        bf = b.start();
        wf = w.start();
        f1f = f1.start();
        f2f = f2.start();

        testWaitForStart(w, mkCnf(w, 0, true, {}, delivery, forwards, snapshot));
    }

    ~DispatchFixture()
    {
        b.stop();
        w.stop();
        f1.stop();
        f2.stop();

        testWaitForStop(w);

        wf.get();
        bf.get();
        f1f.get();
        f2f.get();
    }

    Worker w;
    Broker b;
    Forwarder f1;
    Forwarder f2;

    std::future<void> wf;
    std::future<void> bf;
    std::future<void> f1f;
    std::future<void> f2f;
};


struct DeliveryFixture
{
    DeliveryFixture()
        : w{Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker.net"sv)}
        , b{Uuid::createNamespaceUuid(Uuid::Ns::Dns, "broker.net"sv)}
    {
        const std::vector<std::string> delivery{"ipc:///tmp/delivery_b1", "ipc:///tmp/delivery_b2"};
        const std::vector<std::string> snapshot{"ipc:///tmp/broker_snapshot"};
        const std::vector<std::string> dispatch{"ipc:///tmp/worker_dispatch"};
        const std::vector<std::string> forwards{"ipc:///tmp/delivery_f1", "ipc:///tmp/delivery_f2"};

        b.setEndpoints(forwards, dispatch, snapshot);
        w.setEndpoints(delivery, dispatch, snapshot);
        f1.setEndpoints(Forwarder::Connect, forwards.at(0), Forwarder::Bind, delivery.at(0));
        f2.setEndpoints(Forwarder::Connect, forwards.at(1), Forwarder::Bind, delivery.at(1));

        bf = b.start();
        wf = w.start();
        f1f = f1.start();
        f2f = f2.start();

        testWaitForStart(w, mkCnf(w, 0, true, {}, delivery, dispatch, snapshot));
    }

    ~DeliveryFixture()
    {
        b.stop();
        w.stop();
        f1.stop();
        f2.stop();

        testWaitForStop(w);

        wf.get();
        bf.get();
        f1f.get();
        f2f.get();
    }

    Worker w;
    Broker b;
    Forwarder f1;
    Forwarder f2;

    std::future<void> wf;
    std::future<void> bf;
    std::future<void> f1f;
    std::future<void> f2f;
};


BOOST_FIXTURE_TEST_CASE(testDispatchRedundantFull, DispatchFixture)
{
    auto t1 = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}, Topic::State};
    auto t2 = Topic{b.uuid(), w.uuid(), 0, "topic2"sv, zmq::Part{"hello2"sv}, Topic::State};

    w.dispatch(t1.name(), t1.data());
    w.dispatch(t2.name(), t2.data());

    testWaitForTopic(w, t1, 1);
    testWaitForTopic(w, t2, 2);
}


BOOST_FIXTURE_TEST_CASE(testDispatchRedundantDegraded, DispatchFixture)
{
    f1.setEnabled(false);

    auto t = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}, Topic::State};
    w.dispatch(t.name(), t.data());

    testWaitForTopic(w, t, 1);
}


BOOST_FIXTURE_TEST_CASE(testDispatchRedundantFault, DispatchFixture)
{
    f1.setEnabled(false);
    f2.setEnabled(false);

    auto t = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}, Topic::State};
    w.dispatch(t.name(), t.data());

    testWaitForTimeout(w);
}


BOOST_FIXTURE_TEST_CASE(testDeliveryRedundantFull, DeliveryFixture)
{
    auto t1 = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}, Topic::State};
    auto t2 = Topic{b.uuid(), w.uuid(), 0, "topic2"sv, zmq::Part{"hello2"sv}, Topic::State};

    w.dispatch(t1.name(), t1.data());
    w.dispatch(t2.name(), t2.data());

    testWaitForTopic(w, t1, 1);
    testWaitForTopic(w, t2, 2);
}


BOOST_FIXTURE_TEST_CASE(testDeliveryRedundantDegraded, DeliveryFixture)
{
    f1.setEnabled(false);

    auto t = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}, Topic::State};
    w.dispatch(t.name(), t.data());

    testWaitForTopic(w, t, 1);
}


BOOST_FIXTURE_TEST_CASE(testDeliveryRedundantFault, DeliveryFixture)
{
    f1.setEnabled(false);
    f2.setEnabled(false);

    auto t = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}, Topic::State};
    w.dispatch(t.name(), t.data());

    // next expected event is connection offline
}


// TODO: tests for redundant snapshot endpoint


BOOST_AUTO_TEST_CASE(testDispatchTwoBrokers)
{
    // Setup w1 bound to b2 and w2 bound to b2.
    Broker b1{Uuid::createNamespaceUuid(Uuid::Ns::Dns, "broker1.net"sv)};
    Broker b2{Uuid::createNamespaceUuid(Uuid::Ns::Dns, "broker2.net"sv)};
    Worker w1{Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker1.net"sv)};
    Worker w2{Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker2.net"sv)};

    b1.setEndpoints(
        {"ipc:///tmp/delivery_b1"},
        {"ipc:///tmp/dispatch_b1"},
        {"ipc:///tmp/snapshot_b1"});
    b2.setEndpoints(
        {"ipc:///tmp/delivery_b2"},
        {"ipc:///tmp/dispatch_b2"},
        {"ipc:///tmp/snapshot_b2"});
    w1.setEndpoints(
        {"ipc:///tmp/delivery_b1"},
        {"ipc:///tmp/dispatch_b1", "ipc:///tmp/dispatch_b2"}, // bridge from w1 to b2},
        {"ipc:///tmp/snapshot_b1"});
    w2.setEndpoints(
        {"ipc:///tmp/delivery_b2"},
        {"ipc:///tmp/dispatch_b2"},
        {"ipc:///tmp/snapshot_b2"});

    auto b1f = b1.start();
    auto b2f = b2.start();
    auto w1f = w1.start();
    auto w2f = w2.start();

    for (auto w : {&w1, &w2}) {
        testWaitForStart(*w, mkCnf(*w, 0,
                                 std::get<0>(w->topicsNames()), //
                                 std::get<1>(w->topicsNames()), //
                                 w->endpointDelivery(),         //
                                 w->endpointDispatch(),         //
                                 w->endpointSnapshot()));
    }

    auto t = Topic{{}, w1.uuid(), 1, "topic"sv, zmq::Part{"hello"sv}, Topic::State};

    w1.dispatch(t.name(), t.data());

    // w2 bound to b2, shall receive topic too.
    testWaitForTopic(w1, t.withBroker(b1.uuid()), t.seqNum());
    testWaitForTopic(w2, t.withBroker(b2.uuid()), t.seqNum());

    b1.stop();
    b2.stop();
    w1.stop();
    w2.stop();

    for (auto w : {&w1, &w2})
        testWaitForStop(*w);

    b1f.get();
    b2f.get();
    w1f.get();
    w2f.get();
}
