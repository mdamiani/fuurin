/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/worker.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpart.h"
#include "fuurin/workerconfig.h"
#include "fuurin/sessionworker.h"
#include "log.h"

#include <chrono>
#include <string_view>


using namespace std::literals::string_view_literals;


namespace fuurin {


Worker::Worker(Uuid id, Topic::SeqN initSequence, const std::string& name)
    : Runner{id, name}
    , zseqs_(std::make_unique<zmq::Socket>(context(), zmq::Socket::PUSH))
    , zseqr_(std::make_unique<zmq::Socket>(context(), zmq::Socket::PULL))
    , seqNum_{initSequence}
    , subscrAll_{true}
{
    // MUST be inproc in order to get instant delivery of messages.
    zseqs_->setEndpoints({"inproc://worker-seqn"});
    zseqr_->setEndpoints({"inproc://worker-seqn"});

    zseqs_->setHighWaterMark(1, 1);
    zseqr_->setHighWaterMark(1, 1);

    zseqs_->setConflate(true);
    zseqr_->setConflate(true);

    zseqr_->bind();
    zseqs_->connect();
}


Worker::~Worker() noexcept
{
}


void Worker::setTopicsAll()
{
    subscrAll_ = true;
    subscrNames_.clear();
}


void Worker::setTopicsNames(const std::vector<Topic::Name>& names)
{
    subscrAll_ = false;
    subscrNames_ = names;
}


std::tuple<bool, const std::vector<Topic::Name>&> Worker::topicsNames() const
{
    return {subscrAll_, subscrNames_};
}


void Worker::dispatch(Topic::Name name, const Topic::Data& data, Topic::Type type)
{
    dispatch(name, zmq::Part{data}, type);
}


void Worker::dispatch(Topic::Name name, Topic::Data& data, Topic::Type type)
{
    dispatch(name, std::move(data), type);
}


void Worker::dispatch(Topic::Name name, Topic::Data&& data, Topic::Type type)
{
    if (!isRunning())
        return;

    sendOperation(Operation::Type::Dispatch,
        Topic{Uuid{}, uuid(), Topic::SeqN{}, name, data, type}
            .toPart());
}


void Worker::sync()
{
    if (!isRunning())
        return;

    sendOperation(Operation::Type::Sync);
}


Event Worker::waitForEvent(std::chrono::milliseconds timeout) const
{
    return waitForEvent(timeout, {});
}


Event Worker::waitForEvent(zmq::Pollable& timeout) const
{
    const auto& ev = Runner::waitForEvent(timeout);

    LOG_DEBUG(log::Arg{name(), uuid().toShortString()},
        log::Arg{"event"sv, Event::toString(ev.notification())},
        log::Arg{"type"sv, Event::toString(ev.type())},
        log::Arg{"size"sv, int(ev.payload().size())});

    return ev;
}


Event Worker::waitForEvent(zmq::Pollable* timeout) const
{
    const auto& ev = Runner::waitForEvent(timeout);

    LOG_DEBUG(log::Arg{name(), uuid().toShortString()},
        log::Arg{"event"sv, Event::toString(ev.notification())},
        log::Arg{"type"sv, Event::toString(ev.type())},
        log::Arg{"size"sv, int(ev.payload().size())});

    return ev;
}


Event Worker::waitForEvent(std::chrono::milliseconds timeout, EventMatchFunc match) const
{
    const auto& ev = Runner::waitForEvent(timeout, match);

    LOG_DEBUG(log::Arg{name(), uuid().toShortString()},
        log::Arg{"event"sv, Event::toString(ev.notification())},
        log::Arg{"type"sv, Event::toString(ev.type())},
        log::Arg{"size"sv, int(ev.payload().size())});

    return ev;
}


bool Worker::waitForStarted(std::chrono::milliseconds timeout) const
{
    const auto match = [](Event::Type evt) {
        return evt == Event::Type::Started;
    };

    return match(waitForEvent(timeout, match).type());
}


bool Worker::waitForStopped(std::chrono::milliseconds timeout) const
{
    const auto match = [](Event::Type evt) {
        return evt == Event::Type::Stopped;
    };

    return match(waitForEvent(timeout, match).type());
}


bool Worker::waitForOnline(std::chrono::milliseconds timeout) const
{
    const auto match = [](Event::Type evt) {
        return evt == Event::Type::Online;
    };

    return match(waitForEvent(timeout, match).type());
}


bool Worker::waitForOffline(std::chrono::milliseconds timeout) const
{
    const auto match = [](Event::Type evt) {
        return evt == Event::Type::Offline;
    };

    return match(waitForEvent(timeout, match).type());
}


std::optional<Topic> Worker::waitForTopic(std::chrono::milliseconds timeout) const
{
    const auto match = [](Event::Type evt) {
        return evt == Event::Type::Delivery || evt == Event::Type::SyncElement;
    };

    const auto& ev = waitForEvent(timeout, match);

    if (!match(ev.type()))
        return {};

    return {Topic::fromPart(ev.payload())};
}


Topic::SeqN Worker::seqNumber() const
{
    // latest message is always available
    // over the socket, because we are using
    // inproc transport.
    zmq::Part r;
    if (zseqr_->tryRecv(&r) != -1)
        seqNum_ = r.toUint64();

    return seqNum_;
}


zmq::Part Worker::prepareConfiguration() const
{
    return WorkerConfig{
        uuid(),
        seqNumber(),
        subscrAll_,
        subscrNames_,
        endpointDelivery(),
        endpointDispatch(),
        endpointSnapshot(),
    }
        .toPart();
}


std::unique_ptr<Session> Worker::createSession() const
{
    return makeSession<WorkerSession>(zseqs_.get());
}

} // namespace fuurin
