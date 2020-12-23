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


#define WORKER_HUGZ "HUGZ"
#define WORKER_UPDT "UPDT"


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

        // works for BROKER_HUGZ, BROKER_UPDT as well.
        sockFrom_->setGroups({WORKER_HUGZ, WORKER_UPDT});
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


WorkerConfig mkCnf(const Worker& w, Topic::SeqN seqn = 0, bool wildc = true,
    const std::vector<Topic::Name>& names = {},
    const std::vector<std::string>& endp1 = {"ipc:///tmp/worker_delivery"},
    const std::vector<std::string>& endp2 = {"ipc:///tmp/worker_dispatch"},
    const std::vector<std::string>& endp3 = {"ipc:///tmp/broker_snapshot"})
{
    return WorkerConfig{w.uuid(), seqn, wildc, names, endp1, endp2, endp3};
}


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
    auto t1 = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}};
    auto t2 = Topic{b.uuid(), w.uuid(), 0, "topic2"sv, zmq::Part{"hello2"sv}};

    w.dispatch(t1.name(), t1.data());
    w.dispatch(t2.name(), t2.data());

    testWaitForTopic(w, t1, 1);
    testWaitForTopic(w, t2, 2);
}


BOOST_FIXTURE_TEST_CASE(testDispatchRedundantDegraded, DispatchFixture)
{
    f1.setEnabled(false);

    auto t = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}};
    w.dispatch(t.name(), t.data());

    testWaitForTopic(w, t, 1);
}


BOOST_FIXTURE_TEST_CASE(testDispatchRedundantFault, DispatchFixture)
{
    f1.setEnabled(false);
    f2.setEnabled(false);

    auto t = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}};
    w.dispatch(t.name(), t.data());

    testWaitForTimeout(w);
}


BOOST_FIXTURE_TEST_CASE(testDeliveryRedundantFull, DeliveryFixture)
{
    auto t1 = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}};
    auto t2 = Topic{b.uuid(), w.uuid(), 0, "topic2"sv, zmq::Part{"hello2"sv}};

    w.dispatch(t1.name(), t1.data());
    w.dispatch(t2.name(), t2.data());

    testWaitForTopic(w, t1, 1);
    testWaitForTopic(w, t2, 2);
}


BOOST_FIXTURE_TEST_CASE(testDeliveryRedundantDegraded, DeliveryFixture)
{
    f1.setEnabled(false);

    auto t = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}};
    w.dispatch(t.name(), t.data());

    testWaitForTopic(w, t, 1);
}


BOOST_FIXTURE_TEST_CASE(testDeliveryRedundantFault, DeliveryFixture)
{
    f1.setEnabled(false);
    f2.setEnabled(false);

    auto t = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}};
    w.dispatch(t.name(), t.data());

    testWaitForTimeout(w);
}


// TODO: tests for redundant snapshot endpoint
