/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fuurin/session.h"
#include "fuurin/zmqcontext.h"
#include "fuurin/zmqsocket.h"
#include "fuurin/zmqpoller.h"
#include "fuurin/zmqpart.h"
#include "fuurin/zmqpartmulti.h"
#include "log.h"


#include <boost/scope_exit.hpp>

#include <string_view>


#define GROUP_EVENTS "EVN"


using namespace std::literals::string_view_literals;


namespace fuurin {

Session::Session(const std::string& name, Uuid id, SessionEnv::token_t token,
    zmq::Context* zctx, zmq::Socket* zfin, zmq::Socket* zoper, zmq::Socket* zevent)
    : name_{name}
    , uuid_{id}
    , token_{token}
    , zctx_{zctx}
    , zfins_{zfin}
    , zopr_{zoper}
    , zevs_{zevent}
{
}


Session::~Session() noexcept = default;


void Session::run()
{
    BOOST_SCOPE_EXIT(this)
    {
        try {
            zfins_->send(zmq::Part{token_});
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


std::unique_ptr<zmq::PollerWaiter> Session::createPoller()
{
    auto poll = new zmq::Poller{zmq::PollerEvents::Type::Read, zopr_};
    return std::unique_ptr<zmq::PollerWaiter>{poll};
}


void Session::operationReady(Operation*)
{
}


void Session::socketReady(zmq::Pollable*)
{
}


Operation Session::recvOperation() noexcept
{
    return recvOperation(zopr_, token_);
}


void Session::sendEvent(Event::Type ev, zmq::Part&& pay)
{
    zevs_->send(zmq::PartMulti::pack(token_,
        Event{ev, Event::Notification::Success, pay}.toPart())
                    .withGroup(GROUP_EVENTS));
}


Operation Session::recvOperation(zmq::Socket* sock, SessionEnv::token_t token) noexcept
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

} // namespace fuurin
