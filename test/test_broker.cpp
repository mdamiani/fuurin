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
#include "fuurin/sessionbroker.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"
#include "fuurin/workerconfig.h"
#include "fuurin/errors.h"
#include "fuurin/uuid.h"

#include <string_view>
#include <chrono>
#include <memory>
#include <vector>


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
public:
    TestBroker()
        : Broker{bid}
    {}

protected:
    class TestBrokerSession;

public:
    static const Uuid wid;
    static const Uuid bid;
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


        bool storeTopic(Topic& t)
        {
            return BrokerSession::storeTopic(t);
        }


        auto getStorageTopic() -> decltype(storTopic_)*
        {
            return &storTopic_;
        }

        auto getStorageWorker() -> decltype(storWorker_)*
        {
            return &storWorker_;
        }


        void testReceiveWorkerCommand(const zmq::Part& payload)
        {
            receiveWorkerCommand(zmq::Part{payload});
        }
    };


public:
    using Broker::Broker;

    std::unique_ptr<Session> createSession() const override
    {
        auto ret = makeSession<TestBrokerSession>();
        const_cast<TestBroker*>(this)->setupSession(static_cast<TestBrokerSession*>(ret.get()));
        return ret;
    }


    void setupSession(TestBrokerSession* s)
    {
        testSession = s;
        testSocket = new TestSocket(context(), zmq::Socket::Type{});
        testSession->setSnapshotSocket(testSocket);
    }


    TestBrokerSession* session()
    {
        return testSession;
    }
};

const Uuid TestBroker::wid = Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker.net"sv);
const Uuid TestBroker::bid = Uuid::createNamespaceUuid(Uuid::Ns::Dns, "broker.net"sv);

} // namespace fuurin


BOOST_AUTO_TEST_CASE(testUuid)
{
    auto id = TestBroker::bid;
    Broker b{id};
    BOOST_TEST(b.uuid() == id);
}


static void testStorage(decltype(TestBroker{}.testSession->getStorageTopic()) st,
    Topic::Name nm, Uuid wid, const Topic& t)
{
    BOOST_TEST((st->find(nm) != st->list().end()));
    BOOST_TEST((st->find(nm)->first == nm));
    BOOST_TEST((st->find(nm)->second.find(wid) != st->find(nm)->second.list().end()));
    BOOST_TEST((st->find(nm)->second.find(wid)->first == wid));
    BOOST_TEST((st->find(nm)->second.find(wid)->second == t));
}


static void testStorage(decltype(TestBroker{}.testSession->getStorageWorker()) st,
    Uuid wid, Topic::SeqN val)
{
    BOOST_TEST((st->find(wid) != st->list().end()));
    BOOST_TEST((st->find(wid)->first == wid));
    BOOST_TEST((st->find(wid)->second == val));
}


BOOST_AUTO_TEST_CASE(testStoreTopicSingle)
{
    TestBroker b;
    auto bf = b.start();
    auto bts = b.testSession;
    auto stt = b.testSession->getStorageTopic();
    auto stw = b.testSession->getStorageWorker();

    Topic::Name nm("hello"sv);
    Topic t{Uuid{}, TestBroker::wid, 5, nm, zmq::Part{"data"sv}, Topic::State};

    BOOST_TEST(stt->empty());
    BOOST_TEST((stt->find(nm) == stt->list().end()));
    BOOST_TEST((stw->find(TestBroker::wid) == stw->list().end()));

    // store a topic.
    BOOST_TEST(bts->storeTopic(t));
    BOOST_TEST(t.seqNum() == 5u);

    // store a new topic
    BOOST_TEST(bts->storeTopic(t.withSeqNum(6u)));
    BOOST_TEST(t.seqNum() == 6u);

    // discard topic
    BOOST_TEST(!bts->storeTopic(Topic{t}.withSeqNum(0u)));
    BOOST_TEST(!bts->storeTopic(Topic{t}.withSeqNum(6u)));

    // test storage
    BOOST_TEST(stt->size() == 1u);
    BOOST_TEST(stw->size() == 1u);

    testStorage(stt, nm, TestBroker::wid, t);
    testStorage(stw, TestBroker::wid, t.seqNum());

    b.stop();
}


BOOST_AUTO_TEST_CASE(testStoreTopicMultiple)
{
    TestBroker b;
    auto bf = b.start();
    auto bts = b.testSession;
    auto stt = b.testSession->getStorageTopic();
    auto stw = b.testSession->getStorageWorker();
    const Uuid wid1 = Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker1.net"sv);
    const Uuid wid2 = Uuid::createNamespaceUuid(Uuid::Ns::Dns, "worker2.net"sv);

    Topic::Name nm1("hello1"sv);
    Topic::Name nm2("hello2"sv);
    Topic t1{Uuid{}, wid1, 5, nm1, zmq::Part{"data"sv}, Topic::State};
    Topic t2{Uuid{}, wid2, 7, nm2, zmq::Part{"data"sv}, Topic::State};

    // store topics
    BOOST_TEST(bts->storeTopic(t1));
    BOOST_TEST(bts->storeTopic(t2));
    BOOST_TEST(t1.seqNum() == 5u);
    BOOST_TEST(t2.seqNum() == 7u);

    BOOST_TEST(stt->size() == 2u);
    BOOST_TEST(stw->size() == 2u);

    testStorage(stt, nm1, wid1, t1);
    testStorage(stt, nm2, wid2, t2);

    testStorage(stw, wid1, t1.seqNum());
    testStorage(stw, wid2, t2.seqNum());

    // update topics
    BOOST_TEST(bts->storeTopic(t1.withSeqNum(9)));
    BOOST_TEST(bts->storeTopic(t2.withSeqNum(11)));
    BOOST_TEST(t1.seqNum() == 9u);
    BOOST_TEST(t2.seqNum() == 11u);

    BOOST_TEST(stt->size() == 2u);
    BOOST_TEST(stw->size() == 2u);

    testStorage(stt, nm1, wid1, t1);
    testStorage(stt, nm2, wid2, t2);

    testStorage(stw, wid1, t1.seqNum());
    testStorage(stw, wid2, t2.seqNum());

    b.stop();
}


const WorkerConfig cnfAll{{}, {}, true, {}, {}, {}, {}};
const WorkerConfig cnfNone{{}, {}, false, {}, {}, {}, {}};

const zmq::Part SREQ = zmq::PartMulti::pack(SessionEnv::BrokerSyncReqst, uint8_t(0), cnfAll.toPart()).withRoutingID(1);
const zmq::Part SREQN = zmq::PartMulti::pack(SessionEnv::BrokerSyncReqst, uint8_t(0), cnfNone.toPart()).withRoutingID(1);

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
            SREQ,
            -1, false, false,
            3, ""sv),
        std::make_tuple("no topics",
            SREQN,
            -1, false, false,
            2, ""sv),
        std::make_tuple("begin would block",
            SREQ,
            1, true, false,
            0, ""sv),
        std::make_tuple("elemn would block",
            SREQ,
            2, true, false,
            1, ""sv),
        std::make_tuple("compl would block",
            SREQ,
            3, true, false,
            2, ""sv),
        std::make_tuple("begin host unreach",
            SREQ,
            1, false, true,
            0, ""sv),
        std::make_tuple("elemn host unreach",
            SREQ,
            2, false, true,
            1, ""sv),
        std::make_tuple("compl host unreach",
            SREQ,
            3, false, true,
            2, ""sv),
        std::make_tuple("other exception",
            SREQ,
            1, false, false,
            0, "ZMQSocketSendFailed"sv),
    }),
    name, payload, errAfter, errWouldBlock, errHostUnreach, wantSize, wantError)
{
    BOOST_TEST_MESSAGE(name);

    const auto testNotif = [](const zmq::Part& p, std::string_view id, uint8_t nn) {
        auto [rep, seq, pay] = zmq::PartMulti::unpack<std::string_view, uint8_t, zmq::Part>(p);
        BOOST_TEST(id == rep);
        BOOST_TEST(nn == seq);
        BOOST_TEST(TestBroker::bid == Uuid::fromPart(pay));
    };
    const auto testTopic = [](const zmq::Part& p, std::string_view id, uint8_t nn, Topic tt) {
        auto [rep, seq, pay] = zmq::PartMulti::unpack<std::string_view, uint8_t, zmq::Part>(p);
        BOOST_TEST(id == rep);
        BOOST_TEST(nn == seq);
        BOOST_TEST(tt == Topic::fromPart(pay));
    };

    TestBroker b;
    auto bf = b.start();
    auto bts = b.testSession;
    auto bsk = b.testSocket;
    auto bpp = &bsk->sentParts;

    Topic t = Topic{}.withSeqNum(1).withData(zmq::Part{"hello"sv});
    bts->storeTopic(t);
    bsk->errAfter = errAfter;
    if (!errWouldBlock) {
        bsk->errHostUnreach = errHostUnreach;
        bsk->errOtherCode = !errHostUnreach;
    }

    if (wantError.empty()) {
        bts->testReceiveWorkerCommand(payload);

        if (wantSize > 0) {
            BOOST_TEST(int(bpp->size()) == wantSize);
            if (bpp->size() >= 1)
                testNotif(bpp->at(0), SessionEnv::BrokerSyncBegin, 0);
            if (bpp->size() >= 2) {
                if (payload == SREQ)
                    testTopic(bpp->at(1), SessionEnv::BrokerSyncElemn, 0, t);
                else if (payload == SREQN)
                    testNotif(bpp->at(1), SessionEnv::BrokerSyncCompl, 0);
            }
            if (bpp->size() >= 3)
                testNotif(bpp->at(2), SessionEnv::BrokerSyncCompl, 0);
        } else {
            BOOST_TEST(int(bpp->empty()));
        }
    } else {
        if (wantError == "ZMQPartAccessFailed"sv) {
            BOOST_REQUIRE_THROW(bts->testReceiveWorkerCommand(payload), err::ZMQPartAccessFailed);
        } else if (wantError == "ZMQSocketSendFailed"sv) {
            BOOST_REQUIRE_THROW(bts->testReceiveWorkerCommand(payload), err::ZMQSocketSendFailed);
        } else {
            BOOST_FAIL("unexpected error type");
        }
    }

    b.stop();
}
