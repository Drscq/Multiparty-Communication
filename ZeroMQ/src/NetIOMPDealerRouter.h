#ifndef NET_IOMP_DEALERROUTER_H
#define NET_IOMP_DEALERROUTER_H

#include "INetIOMP.h"
#include <zmq.hpp>
#include <map>
#include <memory>
#include <stdexcept>
#include <string> // ...existing includes...

/**
 * @brief Implementation of INetIOMP using DEALER/ROUTER sockets.
 */
class NetIOMPDealerRouter : public INetIOMP
{
public:
    NetIOMPDealerRouter(PARTY_ID_T partyId, const std::map<PARTY_ID_T, std::pair<std::string, int>>& partyInfo, int totalParties);
    void init() override;
    void initDealers() override; // Add this method
    void initRouter(); // Add this method
    void sendTo(PARTY_ID_T targetId, const void* data, LENGTH_T length) override;
    size_t receive(PARTY_ID_T& senderId, void* buffer, LENGTH_T maxLength) override;
    size_t dealerReceive(PARTY_ID_T& routerId, void* buffer, LENGTH_T maxLength) override; // Add this method
    void reply(const void* data, LENGTH_T length) override;
    void reply(void* routingIdMsg, const void* data, LENGTH_T length) override;
    void reply(void* routingIdMsg, LENGTH_T size, const void* data, LENGTH_T length) override; // Add this method
    void close() override;
    void sendToAll(const void* data, LENGTH_T length) override; // Ensure this is declared
    std::string getLastRoutingId() const override;
    ~NetIOMPDealerRouter() override;
    
private:
    zmq::context_t m_context;
    zmq::socket_t m_routerSocket; // ROUTER socket
    // Remove the single DEALER socket
    // zmq::socket_t m_dealerSocket;

    PARTY_ID_T m_partyId;
    std::map<PARTY_ID_T, std::pair<std::string, int>> m_partyInfo;
    int m_totalParties;

    // Add DEALER sockets map
    std::map<PARTY_ID_T, std::unique_ptr<zmq::socket_t>> m_dealerSockets;

    // Helper method to construct identity string
    std::string getIdentity(PARTY_ID_T partyId);

    // Store the routing ID of the last received message
    std::string m_lastRoutingId; // Ensure this is correctly updated in receive()
};

#endif // NET_IOMP_DEALERROUTER_H