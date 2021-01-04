/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/runner.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"
#include "fuurin/stopwatch.h"
#include "failure.h"
#include "log.h"

#include <boost/scope_exit.hpp>

#include <cstring>


#define GROUP_EVENTS "EVN"


namespace fuurin {

/**
 * MAIN TASK
 */

Runner::Runner(Uuid id, const std::string& name)
    : name_{name}
    , uuid_(id)
    , zctx_(std::make_unique<zmq::Context>())
    , zops_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR))
    , zopr_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PAIR))
    , zevs_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::RADIO))
    , zevr_(std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::DISH))
    , zfins_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PUSH)}
    , zfinr_{std::make_unique<zmq::Socket>(zctx_.get(), zmq::Socket::PULL)}
    , running_(false)
    , token_(0)
    , endpDelivery_{{"ipc:///tmp/worker_delivery"}}
    , endpDispatch_{{"ipc:///tmp/worker_dispatch"}}
    , endpSnapshot_{{"ipc:///tmp/broker_snapshot"}}
{
    zops_->setEndpoints({"inproc://runner-loop"});
    zopr_->setEndpoints({"inproc://runner-loop"});

    zevs_->setEndpoints({"inproc://runner-events"});
    zevr_->setEndpoints({"inproc://runner-events"});

    zevr_->setGroups({GROUP_EVENTS});

    zopr_->bind();
    zops_->connect();

    zevs_->bind();
    zevr_->connect();

    // MUST be inproc in order to get instant delivery of messages.
    zfins_->setEndpoints({"inproc://runner-terminate"});
    zfinr_->setEndpoints({"inproc://runner-terminate"});

    zfins_->setHighWaterMark(1, 1);
    zfinr_->setHighWaterMark(1, 1);

    zfins_->setConflate(true);
    zfinr_->setConflate(true);

    zfinr_->bind();
    zfins_->connect();
}


Runner::~Runner() noexcept
{
    stop();
}


std::string_view Runner::name() const
{
    return std::string_view(name_.data(), name_.size());
}


Uuid Runner::uuid() const
{
    return uuid_;
}


void Runner::setEndpoints(const std::vector<std::string>& delivery,
    const std::vector<std::string>& dispatch,
    const std::vector<std::string>& snapshot)
{
    endpDelivery_ = delivery;
    endpDispatch_ = dispatch;
    endpSnapshot_ = snapshot;
}


std::vector<std::string> Runner::endpointDelivery() const
{
    return endpDelivery_;
}


std::vector<std::string> Runner::endpointDispatch() const
{
    return endpDispatch_;
}


std::vector<std::string> Runner::endpointSnapshot() const
{
    return endpSnapshot_;
}


bool Runner::isRunning() const noexcept
{
    // latest message is always available
    // over the socket, because we are using
    // inproc transport.
    zmq::Part r;
    if (zfinr_->tryRecv(&r) != -1) {
        if (r.toUint8() == token_)
            running_ = false;
    }

    return running_;
}


zmq::Context* Runner::zmqCtx() const noexcept
{
    return zctx_.get();
}


zmq::Part Runner::prepareConfiguration() const
{
    return zmq::Part{};
}


std::unique_ptr<Runner::Session> Runner::createSession(CompletionFunc onComplete) const
{
    return makeSession<Session>(onComplete);
}


std::future<void> Runner::start()
{
    if (isRunning())
        return std::future<void>();

    ++token_;
    running_ = true;

    bool commit = false;
    BOOST_SCOPE_EXIT(this, &commit)
    {
        if (!commit)
            running_ = false;
    };

    auto ret = std::async(std::launch::async,
        [s = createSession([zfins = zfins_.get(), tok = token_.load()]() {
            zfins->send(zmq::Part{tok});
        })]() {
            s->run();
        });

    sendOperation(Operation::Type::Start, prepareConfiguration());

    commit = true;
    return ret;
}


bool Runner::stop() noexcept
{
    if (!isRunning())
        return false;

    sendOperation(Operation::Type::Stop);
    return true;
}


Event Runner::waitForEvent(std::chrono::milliseconds timeout, EventMatchFunc match) const
{
    return waitForEvent(zmq::Poller{zmq::PollerEvents::Read, 0ms, zevr_.get()},
        fuurin::StopWatch{}, std::bind(&Runner::recvEvent, this), match, timeout);
}


Event Runner::waitForEvent(zmq::PollerWaiter&& pw, Elapser&& dt,
    EventRecvFunc recv, EventMatchFunc match, std::chrono::milliseconds timeout)
{
    for (;;) {
        pw.setTimeout(timeout);

        if (pw.wait().empty())
            break;

        const auto& ev = recv();
        if (ev.notification() == Event::Notification::Timeout ||
            (match && !match(ev.type()))) //
        {
            if (timeout != -1ms) {
                timeout -= dt.elapsed();
                if (timeout <= 0ms)
                    break;
            }

            dt.start();
            continue;
        }

        return ev;
    }

    return {Event::Type::Invalid, Event::Notification::Timeout};
}


Event Runner::recvEvent() const
{
    zmq::Part r;
    const auto n = zevr_->tryRecv(&r);
    if (n == -1) {
        return {Event::Type::Invalid, Event::Notification::Timeout};
    }

    ASSERT(std::strncmp(r.group(), GROUP_EVENTS, sizeof(GROUP_EVENTS)) == 0, "bad event group");

    auto [tok, ev] = zmq::PartMulti::unpack<token_type_t, std::string_view>(r);

    return Event::fromPart(ev)
        .withNotification(tok == token_
                ? Event::Notification::Success
                : Event::Notification::Discard);
}


void Runner::sendOperation(Operation::Type oper) noexcept
{
    sendOperation(oper, zmq::Part());
}


void Runner::sendOperation(Operation::Type oper, zmq::Part&& payload) noexcept
{
    sendOperation(zops_.get(), token_, oper, std::move(payload));
}


void Runner::sendOperation(zmq::Socket* sock, token_type_t token, Operation::Type oper, zmq::Part&& payload) noexcept
{
    try {
        sock->send(zmq::Part{token},
            Operation{oper, Operation::Notification::Success, payload}
                .toPart());
    } catch (const std::exception& e) {
        LOG_FATAL(log::Arg{"runner"sv}, log::Arg{"operation send threw exception"sv},
            log::Arg{std::string_view(e.what())});
    }
}


Operation Runner::recvOperation(zmq::Socket* sock, token_type_t token) noexcept
{
    try {
        zmq::Part tok, oper;
        sock->recv(&tok, &oper);
        return Operation::fromPart(oper)
            .withNotification(tok.toUint8() == token
                    ? Operation::Notification::Success
                    : Operation::Notification::Discard);

    } catch (const std::exception& e) {
        LOG_FATAL(log::Arg{"runner"sv}, log::Arg{"operation recv threw exception"sv},
            log::Arg{std::string_view(e.what())});

        return Operation{};
    }
}


/**
 * ASYNC TASK
 */

Runner::Session::Session(const std::string& name, Uuid id, token_type_t token,
    CompletionFunc onComplete, zmq::Context* zctx, zmq::Socket* zoper, zmq::Socket* zevent)
    : name_{name}
    , uuid_{id}
    , token_{token}
    , docompl_{onComplete}
    , zctx_{zctx}
    , zopr_{zoper}
    , zevs_{zevent}
{
}


Runner::Session::~Session() noexcept = default;


void Runner::Session::run()
{
    BOOST_SCOPE_EXIT(this)
    {
        try {
            if (docompl_)
                docompl_();
        } catch (...) {
            LOG_FATAL(log::Arg{"runner"sv},
                log::Arg{"session on complete action threw exception"sv});
        }
    };

    auto poll = createPoller();

    for (;;) {
        for (auto s : poll->wait()) {
            if (s != zopr_) {
                socketReady(s);
                continue;
            }

            auto oper = recvOperation();

            // filter out old operations
            if (oper.notification() == Operation::Notification::Discard)
                continue;

            operationReady(&oper);

            if (oper.type() == Operation::Type::Stop)
                return;
        }
    }
}


std::unique_ptr<zmq::PollerWaiter> Runner::Session::createPoller()
{
    auto poll = new zmq::Poller{zmq::PollerEvents::Type::Read, zopr_};
    return std::unique_ptr<zmq::PollerWaiter>{poll};
}


void Runner::Session::operationReady(Operation*)
{
}


void Runner::Session::socketReady(zmq::Pollable*)
{
}


Operation Runner::Session::recvOperation() noexcept
{
    return Runner::recvOperation(zopr_, token_);
}


void Runner::Session::sendEvent(Event::Type ev, zmq::Part&& pay)
{
    zevs_->send(zmq::PartMulti::pack(token_,
        Event{ev, Event::Notification::Success, pay}.toPart())
                    .withGroup(GROUP_EVENTS));
}

} // namespace fuurin
