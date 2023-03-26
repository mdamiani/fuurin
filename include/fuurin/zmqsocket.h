/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ZMQSOCKET_H
#define ZMQSOCKET_H

#include "fuurin/zmqpollable.h"

#include <string>
#include <chrono>
#include <tuple>
#include <list>
#include <functional>
#include <type_traits>


namespace fuurin {
namespace zmq {


class Context;
class Part;
class PollerObserver;


/**
 * \brief This class wraps a ZMQ socket.
 * This class is not tread-safe.
 */
class Socket : public Pollable
{
public:
    /// Type of ZMQ socket.
    enum Type
    {
        PAIR,
        PUB,
        SUB,
        REQ,
        REP,
        DEALER,
        ROUTER,
        PULL,
        PUSH,
        SERVER,
        CLIENT,
        RADIO,
        DISH,
    };


public:
    /**
     * \brief Initializes this socket with default values.
     *
     * \param[in] ctx A valid ZMQ context.
     * \param[in] type Type of ZMQ socket.
     */
    explicit Socket(Context* ctx, Type type) noexcept;

    /**
     * \brief Destroys (and closes) this socket.
     *
     * \see close()
     */
    ~Socket() noexcept;

    /**
     * Disable copy.
     */
    ///@{
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    ///@}

    /**
     * This method is thread-safe.
     *
     * \return The underlying raw ZMQ pointer.
     */
    void* zmqPointer() const noexcept override;

    /**
     * \return The first endpoint as description or and empty string.
     */
    std::string description() const override;

    /**
     * \return The \ref Context this socket belongs to.
     */
    Context* context() const noexcept;

    /**
     * \return The type of this \ref Socket.
     */
    Type type() const noexcept;

    /**
     * \brief Sets a \c ZMQ_LINGER value to this socket.
     * The socket linger value is actually applied at connection/bind time.
     *
     * \param[in] value Linger value, a negative value sets an infinite wait.
     * \see linger()
     */
    void setLinger(std::chrono::milliseconds value) noexcept;

    /**
     * \return The linger value.
     * \see setLinger(std::chrono::milliseconds)
     */
    std::chrono::milliseconds linger() const noexcept;

    /**
     * \brief Sets \c ZMQ_SNDHWM and \c ZMQ_RCVHWM values to this socket.
     * The high water mark values are actually applied at connection/bind time.
     *
     * \remark A value of 0 means no limit.
     *
     * \param[in] snd High water mark value for outbound messages.
     * \param[in] rcv High water mark value for inbound messages.
     *
     * \see highWaterMark()
     */
    void setHighWaterMark(int snd, int rcv) noexcept;

    /**
     * \brief Returns high water mark value for outbound and inbound messages.
     * \return A pair of the form <ZMQ_SNDHWM, ZMQ_RCVHWM>.
     *
     * \see setHighWaterMark(int, int)
     */
    std::tuple<int, int> highWaterMark() const noexcept;

    /**
     * \brief Sets \c ZMQ_CONFLATE option to this socket.
     * \see conflate()
     */
    void setConflate(bool val) noexcept;

    /**
     * \return The option \c ZMQ_CONFLATE.
     * \see setConflate(bool)
     */
    bool conflate() const noexcept;

    /**
     * \brief Gets the underlying file descriptor of this socket.
     *
     * \return The option \c ZMQ_FD.
     *
     * \exception ZMQSocketOptionGetFailed The specified \c option ZMQ_FD
     *            could not be get, or if the socket is not \ref isOpen().
     *
     * \see bind
     * \see connect
     */
    // TODO: make file descritor type more generic.
    int fileDescriptor() const;

    /**
     * \brief Sets a \c ZMQ_SUBSCRIBE filter to this socket.
     * The socket subscriptions are actually applied at connection/bind time.
     *
     * \param[in] filters Message filters.
     * \see subscriptions()
     */
    void setSubscriptions(const std::list<std::string>& filters);

    /**
     * \return The list of filters for this socket.
     * \see setSubscriptions()
     */
    const std::list<std::string>& subscriptions() const;

    /**
     * \brief Sets the list of groups to join.
     * Groups are joined at connect/bind time.
     * It shall be set to a #DISH socket.
     *
     * \param[in] groups List of groups to join.
     * \see groups()
     */
    void setGroups(const std::list<std::string>& groups);

    /**
     * \return The list of groups to join.
     * \see setGroups(const std::list<std::string>&)
     */
    const std::list<std::string>& groups() const;

    /**
     * \brief Sets the list of endpoints to connect or bind to.
     *
     * \param[in] endpoints Socket endpoints (must be compatible with its \ref type()).
     * \see connect()
     * \see bind()
     * \see type()
     */
    void setEndpoints(const std::list<std::string>& endpoints);

    /**
     * \return The list of endpoints for this socket.
     * \see setEndpoints()
     */
    const std::list<std::string>& endpoints() const;

    /**
     * \brief Connects/binds a ZMQ socket.
     *
     * The ZMQ socket is first created with \c zmq_socket.
     *
     * Then any subsequent socket option is applied:
     *   - Set ZMQ_LINGER property.
     *   - Set ZMQ_SUBSCRIBE property.
     *
     * Finally the socket is connected or bound to the set endpoints.
     * In case of exceptions, socket is not open (exception safety).
     *
     * \exception ZMQSocketCreateFailed Socket could not be created, or it is not closed.
     * \exception ZMQSocketOptionSetFailed Socket options could not be set.
     * \exception ZMQSocketGroupFailed Socket could not be joined to group.
     * \exception ZMQSocketConnectFailed Socket could not be connected to one endpoint.
     * \exception ZMQSocketBindFailed Socket could not be bound to one endpoint.
     *
     * \see setEndpoints
     * \see openEndpoints()
     * \see connect(std::string)
     * \see bind(std::string, int)
     * \see open()
     * \see close()
     * \see setOption()
     * \see join()
     * \see isOpen()
     */
    ///@{
    void connect();
    void bind();
    ///@}

    /**
     * \brief Closes this ZMQ socket.
     *
     * This method calls \c zmq_close.
     * It panics if this socket could not be closed.
     *
     * \see connect()
     * \see bind()
     * \see open()
     * \see ~Socket
     */
    void close() noexcept;

    /**
     * \return Whether this socket is connected or bound.
     * \see connect()
     * \see bind()
     * \see close()
     */
    bool isOpen() const noexcept override;

    /**
     * \return The list of connected or bound endpoints.
     *
     * In case of connect, this list should be the same of \ref endpoints().
     * Otherwise in case of bind it might differ since bind accepts wildcards
     * (e.g. 'tcp:// *:8000'), so it will be the value returned by the socket
     * option \c ZMQ_LAST_ENDPOINT.
     *
     * \see connect()
     * \see bind()
     */
    const std::list<std::string>& openEndpoints() const;

    /**
     * \brief Sends a ZMQ multipart message.
     *
     * Every passed message \ref Part is nullified one by one,
     * during the send call.
     *
     * \param[in] part First message part to send.
     * \param[in] args More message parts to send.
     *
     * \return Total number of bytes of every part.
     *
     * \exception ZMQSocketSendFailed A part could not be sent.
     *
     * \see trySend(Part&&)
     * \see sendMessageMore(Part *)
     * \see sendMessageLast(Part *)
     */
    ///@{
    template<typename T, typename... Args>
    std::enable_if_t<std::is_same_v<std::decay_t<T>, Part>, int>
    send(T&& part, Args&&... args)
    {
        return sendMessageMore(&part) + send(std::forward<Args>(args)...);
    }

    template<typename T>
    std::enable_if_t<std::is_same_v<std::decay_t<T>, Part>, int>
    send(T&& part)
    {
        return sendMessageLast(&part);
    }
    ///@}

    /**
     * \brief Sends a ZMQ multipart message, without blocking.
     *
     * \param[in] part First message part to send.
     * \param[in] args More message parts to send.
     *
     * \return Total number of bytes of every part,
     *         or -1 if the write would block.
     *
     * \exception ZMQSocketSendFailed A part could not be sent.
     *
     * \see send(Part&&)
     * \see sendMessageTryMore(Part *)
     * \see sendMessageTryLast(Part *)
     */
    ///@{
    template<typename T, typename... Args>
    std::enable_if_t<std::is_same_v<std::decay_t<T>, Part>, int>
    trySend(T&& part, Args&&... args)
    {
        const auto n1 = sendMessageTryMore(&part);
        if (n1 == -1)
            return -1;

        const auto n2 = trySend(std::forward<Args>(args)...);
        if (n2 == -1)
            return -1;

        return n1 + n2;
    }

    template<typename T>
    std::enable_if_t<std::is_same_v<std::decay_t<T>, Part>, int>
    trySend(T&& part)
    {
        return sendMessageTryLast(&part);
    }
    ///@}

    /**
     * \brief Receives a ZMQ multipart message.
     *
     * Every passed message \ref Part is cleared if the are not empty.
     *
     * \param[out] part First message part to fill with the received data.
     * \param[out] args More message parts to receive.
     *
     * \return Total number of bytes of every part.
     *
     * \exception ZMQSocketRecvFailed A part could not be received.
     *
     * \see tryRecv(Part*)
     * \see recvMessageMore(Part *)
     * \see recvMessageLast(Part *)
     */
    ///@{
    template<typename... Args>
    int recv(Part* part, Args&&... args)
    {
        return recvMessageMore(part) + recv(std::forward<Args>(args)...);
    }

    int recv(Part* part)
    {
        return recvMessageLast(part);
    }
    ///@}

    /**
     * \brief Receives a ZMQ multipart message, without blocking.
     *
     * \param[out] part First message part to fill with the received data.
     * \param[out] args More message parts to receive.
     *
     * \return Total number of bytes of every part,
     *         or -1 if the read would block.
     *
     * \exception ZMQSocketRecvFailed A part could not be received.
     *
     * \see recv(Part*)
     * \see recvMessageTryMore(Part *)
     * \see recvMessageTryLast(Part *)
     */
    ///@{
    template<typename... Args>
    int tryRecv(Part* part, Args&&... args)
    {
        const auto n1 = recvMessageTryMore(part);
        if (n1 == -1)
            return -1;

        const auto n2 = tryRecv(std::forward<Args>(args)...);
        if (n2 == -1)
            return -1;

        return n1 + n2;
    }

    int tryRecv(Part* part)
    {
        return recvMessageTryLast(part);
    }
    ///@}


protected:
    /**
     * \brief Sends a ZMQ message part to this socket.
     *
     * When the more multiple parts are being sent,
     * then flag ZMQ_SNDMORE is passed to \c zmq_msg_send.
     *
     * The part is nullified by \c zmq_msg_send, after the call.
     *
     * \param[in] part Message part to send.
     *
     * \return Number of bytes of the part.
     *
     * \exception ZMQSocketSendFailed The part could not be sent.
     */
    ///@{
    virtual int sendMessageMore(Part* part);
    virtual int sendMessageLast(Part* part);
    ///@}

    /**
     * \brief Sends a ZMQ message part to this socket, without blocking.
     *
     * The flag \c ZMQ_DONTWAIT is passed to \c zmq_msg_send.
     *
     * \param[in] part Message part to send.
     *
     * \return Number of bytes of the part,
     *         or -1 if the write would block.
     *
     * \exception ZMQSocketSendFailed The part could not be sent.
     *
     * \see sendMessageMore
     * \see sendMessageLast
     */
    ///@{
    virtual int sendMessageTryMore(Part* part);
    virtual int sendMessageTryLast(Part* part);
    ///@}

    /**
     * \brief Receives a ZMQ multipart message.
     *
     * The behaviour of this function if the same of \c zmq_msg_recv.
     * The passed part is filled with data read from this socket, as is,
     * and possibly cleared if its contents is not empty.
     *
     * \param[out] part A single message part to fill with the received data.
     *
     * \return Number of bytes in the message part.
     *
     * \exception ZMQSocketRecvFailed The part could not be received.
     */
    ///@{
    virtual int recvMessageMore(Part* part);
    virtual int recvMessageLast(Part* part);
    ///@}

    /**
     * \brief Receives a ZMQ multipart message, without blocking.
     *
     * The flag \c ZMQ_DONTWAIT is passed to \c zmq_msg_recv.
     *
     * \param[out] part A single message part to fill with the received data.
     *
     * \return Number of bytes in the message part,
     *         or -1 if the write would block.
     *
     * \exception ZMQSocketRecvFailed The part could not be received.
     */
    ///@{
    virtual int recvMessageTryMore(Part* part);
    virtual int recvMessageTryLast(Part* part);
    ///@}


private:
    /**
     * \brief Opens a ZMQ socket.
     *
     * \param[in] action Either \c connect or \c bind.
     *
     * \see connect()
     * \see bind()
     */
    void open(std::function<void(std::string)> action);

    /**
     * \brief Connects a ZMQ socket.
     *
     * This method calls \c zmq_connect.
     *
     * \param[in] endpoint A ZMQ endpoint to use.
     * \exception ZMQSocketConnectFailed The socket could not be connected to the endpoint.
     *
     * \see open
     * \see close
     */
    void connect(const std::string& endpoint);

    /**
     * \brief Binds a ZMQ socket.
     *
     * This method calls \c zmq_bind.
     * Possibly the bind action is retried until the specified \c timeout,
     * in case of EADDRINUSE ('address already in use') error, that might
     * happen in case of fast respawing of the same application.
     *
     * \param[in] endpoint A ZMQ endpoint to use.
     * \param[in] timeout Timeout in milliseconds until retry is performed,
     *                    before returning an error. When passing -1,
     *                    retry is not performed at all. When passing 0,
     *                    retring is done forever.
     * \exception ZMQSocketBindFailed The socket could not be bound to the endpoint.
     *
     * \see open
     * \see close
     */
    void bind(const std::string& endpoint, int timeout = -1);

    /**
     * \brief Sets a ZMQ \c option to this \ref Socket.
     *
     * Valid options are listed in the manual of ZMQ function \c zmq_setsockopt.
     *
     * \param[in] option Option to set.
     * \param[in] value Value to set.
     * \exception ZMQSocketOptionSetFailed The specified \c option and \c value could not be set.
     */
    template<typename T>
    void setOption(int option, const T& value);

    /**
     * \brief Gets a ZMQ \c option of this \ref Socket.
     *
     * Valid options are listed in the manual of ZMQ function \c zmq_getsockopt.
     *
     * \param[in] option Option to query.
     * \exception ZMQSocketOptionGetFailed The specified \c option value could not be get.
     */
    template<typename T>
    T getOption(int option) const;

    /**
     * \brief Joins a RADIO/DISH group.
     *
     * It shall be called on a #DISH socket.
     * This method calls \c zmq_join.
     *
     * \param[in] group Group to join.
     *
     * \exception ZMQSocketGroupFailed The group could not be joined.
     */
    void join(const std::string& group);


private:
    Context* const ctx_; ///< ZMQ context this socket belongs to.
    const Type type_;    ///< ZMQ type of socket.
    void* ptr_;          ///< ZMQ socket.

    std::chrono::milliseconds linger_;     ///< Linger value.
    int hwmsnd_;                           ///< High water mark for outbound messages.
    int hwmrcv_;                           ///< High water mark for inbound messages.
    bool conflate_;                        ///< Conflate option.
    std::list<std::string> subscriptions_; ///< List of subscriptions.
    std::list<std::string> groups_;        ///< List of groups.
    std::list<std::string> endpoints_;     ///< List of endpoints to connect/bind.
    std::list<std::string> openEndpoints_; ///< List of connected/bound endpoints.
};
} // namespace zmq
} // namespace fuurin

#endif // ZMQSOCKET_H
