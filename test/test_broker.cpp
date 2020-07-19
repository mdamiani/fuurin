/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE broker
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/mpl/list.hpp>
#include <benchmark/benchmark.h>

#include "fuurin/broker.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"
#include "fuurin/errors.h"

#include <string_view>
#include <chrono>
#include <memory>
#include <vector>

#define BROKER_SYNC_REQST "SYNC"sv
#define BROKER_SYNC_BEGIN "BEGN"sv
#define BROKER_SYNC_ELEMN "ELEM"sv
#define BROKER_SYNC_COMPL "SONC"sv


using namespace fuurin;
namespace utf = boost::unit_test;
namespace bdata = utf::data;

using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;


namespace fuurin {
class TestSocket : public zmq::Socket
{
public:
    using Socket::Socket;

public:
    int errAfter = 0;
    bool errHostUnreach = false;
    bool errOtherCode = false;
    std::vector<zmq::Part> sentParts;


protected:
    int sendMessageTryLast(zmq::Part* part)
    {
        if (errAfter > 0)
            errAfter--;

        if (errAfter == 0) {
            if (errHostUnreach)
                throw ERROR(ZMQSocketSendFailed, "", log::Arg{log::ec_t{EHOSTUNREACH}});
            else if (errOtherCode)
                throw ERROR(ZMQSocketSendFailed, "", log::Arg{log::ec_t{ENOTCONN}});
            else
                return -1;
        }

        sentParts.push_back(*part);

        return part->size();
    }
};


class TestBroker : public Broker
{
protected:
    class TestBrokerSession;

public:
    TestSocket* testSocket = nullptr;
    TestBrokerSession* testSession;


protected:
    class TestBrokerSession : public BrokerSession
    {
    public:
        using BrokerSession::BrokerSession;


        void setSnapshotSocket(zmq::Socket* s)
        {
            const_cast<std::unique_ptr<zmq::Socket>&>(zsnapshot_).reset(s);
        }


        void setSnapshot(std::string data)
        {
            storage_.push_back(data);
        }


        void testReceiveWorkerCommand(zmq::Part&& payload)
        {
            receiveWorkerCommand(std::move(payload));
        }
    };


public:
    using Broker::Broker;

    std::unique_ptr<Session> createSession(CompletionFunc onComplete) const override
    {
        auto ret = makeSession<TestBrokerSession>(onComplete);
        const_cast<TestBroker*>(this)->setupSession(static_cast<TestBrokerSession*>(ret.get()));
        return ret;
    }


    void setupSession(TestBrokerSession* s)
    {
        testSession = s;
        testSocket = new TestSocket(nullptr, zmq::Socket::Type{});
        testSession->setSnapshotSocket(testSocket);
    }


    TestBrokerSession* session()
    {
        return testSession;
    }
};
} // namespace fuurin


BOOST_DATA_TEST_CASE(testReceiverWorkerSync,
    bdata::make({
        std::make_tuple("payload empty",
            zmq::Part{},
            -1, false, false,
            0, "ZMQPartAccessFailed"sv),
        std::make_tuple("payload bad request",
            zmq::PartMulti::pack("BADREQ"sv, uint8_t(0), zmq::Part{}),
            -1, false, false,
            0, ""sv),
        std::make_tuple("payload ok",
            zmq::PartMulti::pack(BROKER_SYNC_REQST, uint8_t(0), zmq::Part{}).withRoutingID(1),
            -1, false, false,
            3, ""sv),
        std::make_tuple("begin would block",
            zmq::PartMulti::pack(BROKER_SYNC_REQST, uint8_t(0), zmq::Part{}).withRoutingID(1),
            1, true, false,
            0, ""sv),
        std::make_tuple("elemn would block",
            zmq::PartMulti::pack(BROKER_SYNC_REQST, uint8_t(0), zmq::Part{}).withRoutingID(1),
            2, true, false,
            1, ""sv),
        std::make_tuple("compl would block",
            zmq::PartMulti::pack(BROKER_SYNC_REQST, uint8_t(0), zmq::Part{}).withRoutingID(1),
            3, true, false,
            2, ""sv),
        std::make_tuple("begin host unreach",
            zmq::PartMulti::pack(BROKER_SYNC_REQST, uint8_t(0), zmq::Part{}).withRoutingID(1),
            1, false, true,
            0, ""sv),
        std::make_tuple("elemn host unreach",
            zmq::PartMulti::pack(BROKER_SYNC_REQST, uint8_t(0), zmq::Part{}).withRoutingID(1),
            2, false, true,
            1, ""sv),
        std::make_tuple("compl host unreach",
            zmq::PartMulti::pack(BROKER_SYNC_REQST, uint8_t(0), zmq::Part{}).withRoutingID(1),
            3, false, true,
            2, ""sv),
        std::make_tuple("other exception",
            zmq::PartMulti::pack(BROKER_SYNC_REQST, uint8_t(0), zmq::Part{}).withRoutingID(1),
            1, false, false,
            0, "ZMQSocketSendFailed"sv),
    }),
    name, payload, errAfter, errWouldBlock, errHostUnreach, wantSize, wantError)
{
    BOOST_TEST_MESSAGE(name);

    const auto testPart = [](const zmq::Part& p, std::string_view id, uint8_t nn, std::string_view vv) {
        auto [rep, seq, pay] = zmq::PartMulti::unpack<std::string_view, uint8_t, zmq::Part>(p);
        BOOST_TEST(id == rep);
        BOOST_TEST(nn == seq);
        BOOST_TEST(vv == pay.toString());
    };

    TestBroker b;
    auto bf = b.start();

    b.testSession->setSnapshot("hello");
    b.testSocket->errAfter = errAfter;
    if (!errWouldBlock) {
        b.testSocket->errHostUnreach = errHostUnreach;
        b.testSocket->errOtherCode = !errHostUnreach;
    }

    if (wantError.empty()) {
        b.testSession->testReceiveWorkerCommand(zmq::Part{payload});

        if (wantSize > 0) {
            BOOST_TEST(int(b.testSocket->sentParts.size()) == wantSize);
            if (b.testSocket->sentParts.size() >= 1)
                testPart(b.testSocket->sentParts.at(0), BROKER_SYNC_BEGIN, 0, ""sv);
            if (b.testSocket->sentParts.size() >= 2)
                testPart(b.testSocket->sentParts.at(1), BROKER_SYNC_ELEMN, 0, "hello"sv);
            if (b.testSocket->sentParts.size() >= 3)
                testPart(b.testSocket->sentParts.at(2), BROKER_SYNC_COMPL, 0, ""sv);
        } else {
            BOOST_TEST(int(b.testSocket->sentParts.empty()));
        }
    } else {
        if (wantError == "ZMQPartAccessFailed"sv) {
            BOOST_REQUIRE_THROW(b.testSession->testReceiveWorkerCommand(zmq::Part{payload}), err::ZMQPartAccessFailed);
        } else if (wantError == "ZMQSocketSendFailed"sv) {
            BOOST_REQUIRE_THROW(b.testSession->testReceiveWorkerCommand(zmq::Part{payload}), err::ZMQSocketSendFailed);
        } else {
            BOOST_FAIL("unexpected error type");
        }
    }

    b.stop();
}
