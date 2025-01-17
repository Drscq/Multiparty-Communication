#ifndef NET_IOMP_H
#define NET_IOMP_H

#include <zmq.hpp>
#include <map>
#include <memory>
#include <stdexcept>
#include "config.h"

/**
 * @brief A ZeroMQ-based class for multi-party communication using
 *        a Request-Reply (REQ/REP) pattern. This version supports binary data
 *        and identifies the sender's party ID.
 */
class NetIOMP
{
public:
    /**
     * @brief Constructor
     * @param partyId    The ID of this party (unique integer).
     * @param partyInfo  A mapping from party ID -> (ip, port).
     */
    NetIOMP(PARTY_ID_T partyId, const std::map<PARTY_ID_T, std::pair<std::string, int>>& partyInfo);

    /**
     * @brief Initializes the communication sockets.
     */
    void init();

    /**
     * @brief Sends binary data to a specific party.
     * @param targetId   The party ID to which we want to send data.
     * @param data       Pointer to the binary data to send.
     * @param length     Length of the binary data in bytes.
     */
    void sendTo(PARTY_ID_T targetId, const void* data, LENGTH_T length);

    /**
     * @brief Receives binary data from another party.
     * @param senderId   Output parameter to store the sender's party ID.
     * @param buffer     Pointer to the buffer to store received data.
     * @param maxLength  Maximum length of the buffer.
     * @return Size of the received data in bytes.
     */
    size_t receive(PARTY_ID_T& senderId, void* buffer, LENGTH_T maxLength);

    /**
     * @brief Replies with binary data to the last received message.
     * @param data       Pointer to the binary data to send as a reply.
     * @param length     Length of the binary data in bytes.
     */
    void reply(const void* data, size_t length);

    /**
     * @brief Polls for incoming messages and processes them.
     * @param timeout   Timeout in milliseconds for polling.
     * @return True if a message was received, false otherwise.
     */
    bool pollAndProcess(int timeout = -1);

    /**
     * @brief Closes all sockets.
     */
    void close();

    /**
     * @brief Destructor.
     */
    ~NetIOMP();

private:
    PARTY_ID_T m_partyId;
    std::map<PARTY_ID_T, std::pair<std::string, int>> m_partyInfo;

    zmq::context_t m_context;
    std::unique_ptr<zmq::socket_t> m_repSocket;
    std::map<PARTY_ID_T, std::unique_ptr<zmq::socket_t>> m_reqSockets;
};

#endif // NET_IOMP_H
