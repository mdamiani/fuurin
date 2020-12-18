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
        , dispatchFrom_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::DISH)}
        , dispatchTo_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::RADIO)}
    {
        stopSnd_->setEndpoints({"inproc://forwarder-stop"});
        stopRcv_->setEndpoints({"inproc://forwarder-stop"});

        stopRcv_->bind();
        stopSnd_->connect();

        dispatchFrom_->setGroups({WORKER_HUGZ, WORKER_UPDT});
    }

    void setEndpoints(const std::string& dispFrom, const std::string& dispTo)
    {
        dispatchFrom_->setEndpoints({dispFrom});
        dispatchTo_->setEndpoints({dispTo});
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
        dispatchFrom_->bind();
        dispatchTo_->connect();

        zmq::Poller poll{zmq::PollerEvents::Type::Read, dispatchFrom_.get(), stopRcv_.get()};

        for (;;) {
            for (auto s : poll.wait()) {
                if (s == dispatchFrom_.get()) {
                    zmq::Part p;
                    dispatchFrom_->recv(&p);
                    dispatchTo_->send(p);

                } else if (s == stopRcv_.get()) {
                    zmq::Part p;
                    stopRcv_->recv(&p);
                    return;
                }
            }
        }
    }


private:
    const std::unique_ptr<zmq::Context> zctx_;
    const std::unique_ptr<zmq::Socket> stopSnd_;
    const std::unique_ptr<zmq::Socket> stopRcv_;
    const std::unique_ptr<zmq::Socket> dispatchFrom_;
    const std::unique_ptr<zmq::Socket> dispatchTo_;
};


WorkerConfig mkCnf(const Worker& w, Topic::SeqN seqn = 0,
    const std::vector<Topic::Name>& names = {},
    const std::vector<std::string>& endp1 = {"ipc:///tmp/worker_delivery"},
    const std::vector<std::string>& endp2 = {"ipc:///tmp/worker_dispatch"},
    const std::vector<std::string>& endp3 = {"ipc:///tmp/broker_snapshot"})
{
    return WorkerConfig{w.uuid(), seqn, names, endp1, endp2, endp3};
}


BOOST_AUTO_TEST_CASE(testRedundantDispatch)
{
    Broker b;
    Worker w;
    Forwarder f1;
    Forwarder f2;

    const std::vector<std::string> delivery{"ipc:///tmp/worker_delivery"};
    const std::vector<std::string> snapshot{"ipc:///tmp/broker_snapshot"};
    const std::vector<std::string> dispatch{"ipc:///tmp/dispatch_b1", "ipc:///tmp/dispatch_b2"};
    const std::vector<std::string> forwards{"ipc:///tmp/dispatch_f1", "ipc:///tmp/dispatch_f2"};

    b.setEndpoints(delivery, dispatch, snapshot);
    w.setEndpoints(delivery, forwards, snapshot);
    f1.setEndpoints("ipc:///tmp/dispatch_f1", "ipc:///tmp/dispatch_b1");
    f2.setEndpoints("ipc:///tmp/dispatch_f2", "ipc:///tmp/dispatch_b2");

    auto bf = b.start();
    auto wf = w.start();
    auto f1f = f1.start();
    auto f2f = f2.start();

    testWaitForStart(w, mkCnf(w, 0, {}, delivery, forwards, snapshot));

    auto t1 = Topic{b.uuid(), w.uuid(), 0, "topic1"sv, zmq::Part{"hello1"sv}};
    auto t2 = Topic{b.uuid(), w.uuid(), 0, "topic2"sv, zmq::Part{"hello2"sv}};

    w.dispatch(t1.name(), t1.data());
    w.dispatch(t2.name(), t2.data());

    testWaitForTopic(w, t1, 1);
    testWaitForTopic(w, t2, 2);
    testWaitForTimeout(w);

    b.stop();
    w.stop();
    f1.stop();
    f2.stop();

    testWaitForStop(w);
}
