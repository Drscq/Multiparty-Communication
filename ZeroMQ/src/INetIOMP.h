#ifndef INET_IOMP_H
#define INET_IOMP_H

#include <cstdint>  // For fixed-width integer types
#include <map>
#include <string>
#include "config.h"

/**
 * @brief Abstract base class for multi-party communication.
 */
class INetIOMP
{
public:
    virtual ~INetIOMP() = default;

    /**
     * @brief Initializes the communication sockets.
     */
    virtual void init() = 0;

    /**
     * @brief Initializes the DEALER sockets for all other parties.
     */
    virtual void initDealers() = 0;

    /**
     * @brief Sends binary data to a specific party.
     * @param targetId   The party ID to which we want to send data.
     * @param data       Pointer to the binary data to send.
     * @param length     Length of the binary data in bytes.
     */
    virtual void sendTo(PARTY_ID_T targetId, const void* data, LENGTH_T length) = 0;

    /**
     * @brief Sends binary data to all parties.
     * @param data       Pointer to the binary data to send.
     * @param length     Length of the binary data in bytes.
     */
    virtual void sendToAll(const void* data, LENGTH_T length) = 0;

    /**
     * @brief Receives binary data from another party.
     * @param senderId   Output parameter to store the sender's party ID.
     * @param buffer     Pointer to the buffer to store received data.
     * @param maxLength  Maximum length of the buffer.
     * @return Size of the received data in bytes.
     */
    virtual size_t receive(PARTY_ID_T& senderId, void* buffer, LENGTH_T maxLength) = 0;

    /**
     * @brief Receives binary data from a DEALER socket.
     * @param routerId   Output parameter to store the router's party ID.
     * @param buffer     Pointer to the buffer to store received data.
     * @param maxLength  Maximum length of the buffer.
     * @return Size of the received data in bytes.
     */
    virtual size_t dealerReceive(PARTY_ID_T& routerId, void* buffer, LENGTH_T maxLength) = 0;

    /**
     * @brief Replies with binary data to the last received message.
     * @param data       Pointer to the binary data to send as a reply.
     * @param length     Length of the binary data in bytes.
     */
    virtual void reply(const void* data, LENGTH_T length) = 0;

    virtual void reply(void* routingIdMsg, const void* data, LENGTH_T length) = 0;

    /**
     * @brief Closes all sockets.
     */
    virtual void close() = 0;

    /**
     * @brief Gets the last routing ID.
     * @return The last routing ID as a string.
     */
    virtual std::string getLastRoutingId() const = 0;
};

#endif // INET_IOMP_H