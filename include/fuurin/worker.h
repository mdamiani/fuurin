/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_WORKER_H
#define FUURIN_WORKER_H

#include <memory>
#include <future>
#include <atomic>


namespace fuurin {

namespace zmq {
class Context;
class Socket;
} // namespace zmq


class Worker
{
public:
    Worker();
    virtual ~Worker() noexcept;

    std::future<void> start();
    bool stop() noexcept;
    bool isRunning() const noexcept;


private:
    enum Command : uint8_t
    {
        Stop,
    };


private:
    void run(uint8_t token);


private:
    std::unique_ptr<zmq::Context> zctx_;
    std::unique_ptr<zmq::Socket> zcmds_;
    std::unique_ptr<zmq::Socket> zcmdr_;
    std::atomic<bool> running_;
    uint8_t token_;
};

} // namespace fuurin

#endif // FUURIN_WORKER_H
