/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_TOKENPOOL_H
#define FUURIN_TOKENPOOL_H

#include <memory>
#include <optional>


namespace fuurin {

namespace zmq {
class Context;
class Socket;
} // namespace zmq


/**
 * \brief Thread-safe class to put and get tokens.
 */
class TokenPool
{
public:
    using id_t = uint32_t;


public:
    /**
     * \brief Initializes an empty object with no tokens.
     */
    TokenPool();

    /**
     * \brief Initializes an object with sequential tokens.
     *
     * \param[in] idMin Miminum ID value of tokens.
     * \param[in] idMax Maximum ID value of tokens.
     *
     * \see put(id_t)
     */
    TokenPool(id_t idMin, id_t idMax);

    /**
     * \brief Destructor.
     */
    ~TokenPool() noexcept;

    /**
     * \brief Puts a token into the pool.
     *
     * \param[in] id Value of the token.
     */
    void put(id_t id);

    /**
     * \brief Gets a token from the pool.
     *
     * This method blocks until a token is available in the pool.
     *
     * \return Value of the token.
     *
     * \see put(id_t)
     * \see tryGet()
     */
    id_t get();

    /**
     * \brief Gets a token from the pool.
     *
     * This method return an emtpy value if this pool contains no tokens,
     * without blocking.
     *
     * \return An empty value in case of no tokens are available.
     *
     * \see put(id_t)
     * \see get()
     */
    std::optional<id_t> tryGet();


private:
    std::unique_ptr<zmq::Context> zmqCtx_; ///< ZMQ Context.
    std::unique_ptr<zmq::Socket> zmqPut_;  ///< ZMQ put end side.
    std::unique_ptr<zmq::Socket> zmqGet_;  ///< ZMQ get end side.
};

} // namespace fuurin

#endif // FUURIN_TOKENPOOL_H
