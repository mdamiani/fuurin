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

#include "fuurin/broker.h"
#include "fuurin/worker.h"
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
        , dispatchFrom_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::DISH)}
        , dispatchTo_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::RADIO)}
    {
        stopSnd_->setEndpoints({"inproc://forwarder-stop"});
        stopRcv_->setEndpoints({"inproc://forwarder-stop"});

        stopRcv_->bind();
        stopSnd_->connect();
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


BOOST_AUTO_TEST_CASE(testRedundantDispatch)
{
    Broker b;
    Worker w;
    Forwarder f1;
    Forwarder f2;

    const std::vector<std::string> delivery{"ipc:///tmp/worker_delivery"};
    const std::vector<std::string> snapshot{"ipc:///tmp/broker_snapshot"};

    b.setEndpoints(delivery, {"ipc:///tmp/dispatch_b1", "ipc:///tmp/dispatch_b2"}, snapshot);
    w.setEndpoints(delivery, {"ipc:///tmp/dispatch_f1", "ipc:///tmp/dispatch_f2"}, snapshot);
    f1.setEndpoints("ipc:///tmp/dispatch_f1", "ipc:///tmp/dispatch_b1");
    f2.setEndpoints("ipc:///tmp/dispatch_f2", "ipc:///tmp/dispatch_b2");

    auto bf = b.start();
    auto wf = w.start();
    auto f1f = f1.start();
    auto f2f = f2.start();

    b.stop();
    w.stop();
    f1.stop();
    f2.stop();
}
