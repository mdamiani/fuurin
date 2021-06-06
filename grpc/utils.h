/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef GRPC_UTILS_H
#define GRPC_UTILS_H

#include <vector>
#include <string>
#include <chrono>
#include <future>


namespace fuurin {
class Runner;
}


namespace utils {

///< Enpoints triplet.
struct Endpoints
{
    std::vector<std::string> delivery; ///< Endpoints for delivery.
    std::vector<std::string> dispatch; ///< Endpoints for dispatch.
    std::vector<std::string> snapshot; ///< Endpoints for snapshot.
};


/**
 * \brief Parses endpoints starting at specified index of command line arguments.
 *
 * \param[in] argc Command line \c argc.
 * \param[in] argv Command line \c argv.
 * \param[in] startIdx Index of \c argv at which parsing is started.
 * \param[in] runner Runner to apply endpoints.
 *
 * \see parseArgsEndpoints
 * \see applyArgsEndpoints
 *
 * \return The actual \c runner endpoints.
 */
Endpoints parseAndApplyArgsEndpoints(int argc, char** argv, int startIdx, fuurin::Runner* runner);


/**
 * \brief Parses endpoints starting at specified index of command line arguments.
 *
 * \param[in] argc Command line \c argc.
 * \param[in] argv Command line \c argv.
 * \param[in] startIdx Index of \c argv at which parsing is started.
 *
 * \return Connection endpoints, or an empty value.
 */
Endpoints parseArgsEndpoints(int argc, char** argv, int startIdx);


/**
 * \brief Parses gRPC server address at the specified index of command line arguments.
 *
 * \param[in] argc Command line \c argc.
 * \param[in] argv Command line \c argv.
 * \param[in] startIdx Index of \c argv at which parsing is started.
 *
 * \return Server address, or "localhost:50051" as default address.
 */
std::string parseArgsServerAddress(int argc, char** argv, int startIdx);


/**
 * \brief Applies connection endpoints to a runner.
 *
 * If every passed endpoint is empty,
 * then no changes are applied.
 *
 * \param[in] endpts Connection endpoints.
 * \param[in] runner Runner to apply endpoints.
 *
 * \return The actual \c runner endpoints.
 */
Endpoints applyArgsEndpoints(Endpoints endpts, fuurin::Runner* runner);


/**
 * \brief Prints the passed endpoint.
 *
 * \param[in] endpts Endpoints to print.
 */
void printArgsEndpoints(const Endpoints& endpts);


/**
 * \brief Prints the server address.
 *
 * \param[in] addr Server address.
 */
void printArgsServerAddress(const std::string& addr);


/**
 * \brief Waits for result from the passed \c future.
 *
 * In case of errors, it prints out the exception message.
 *
 * \param[in] future Future to wait for.
 * \param[in] timeout Timeout to get result. Negative value will cause no expiration.
 */
void waitForResult(std::future<void>* future, std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));

} // namespace utils

#endif // GRPC_UTILS_H
