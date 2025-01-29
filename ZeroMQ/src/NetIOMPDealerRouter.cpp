#include "NetIOMPDealerRouter.h"
#include <cstring>
#include <iostream>
#include <thread>

NetIOMPDealerRouter::NetIOMPDealerRouter(PARTY_ID_T partyId,
                                         const std::map<PARTY_ID_T, std::pair<std::string, int>>& partyInfo, int totalParties)
    // Reordered initializer list to match member declaration order: m_context, m_routerSocket, m_partyId, m_partyInfo
    : m_context(1),
      m_routerSocket(m_context, ZMQ_ROUTER),
      m_partyId(partyId),          // Moved before m_partyInfo
      m_partyInfo(partyInfo),
      m_totalParties(totalParties)
{
    // if (m_partyInfo.find(m_partyId) == m_partyInfo.end()) {
    //     throw std::runtime_error("[NetIOMPDealerRouter] Party ID not found in partyInfo map.");
    // }
}

std::string NetIOMPDealerRouter::getIdentity(PARTY_ID_T partyId)
{
    return "Party" + std::to_string(partyId);
}

void NetIOMPDealerRouter::init()
{
    // Setup the ROUTER (server) socket
    int linger = 0;
    m_routerSocket.set(zmq::sockopt::linger, linger);
    int rcvTimeout = 300; // ms
    m_routerSocket.set(zmq::sockopt::rcvtimeo, rcvTimeout);

    auto [myIp, myPort] = m_partyInfo.at(m_partyId);
    std::string bindEndpoint = "tcp://" + myIp + ":" + std::to_string(myPort);

    std::cout << "Party " << m_partyId << " binding to " << bindEndpoint << "\n";
    m_routerSocket.bind(bindEndpoint);

    // Setup DEALER (client) sockets for all other parties
    for (const auto& [pid, ipPort] : m_partyInfo) {
        if (pid == m_partyId || pid > m_totalParties)
            continue;

        auto [ip, port] = ipPort;
        std::string connectEndpoint = "tcp://" + ip + ":" + std::to_string(port);

        auto dealerSocket = std::make_unique<zmq::socket_t>(m_context, ZMQ_DEALER);
        dealerSocket->set(zmq::sockopt::linger, linger);

        // Set a unique identity for the DEALER socket by including target party ID
        std::string identity = getIdentity(m_partyId) + "_to_" + std::to_string(pid);
        dealerSocket->set(zmq::sockopt::routing_id, identity);

        dealerSocket->connect(connectEndpoint);

        m_dealerSockets[pid] = std::move(dealerSocket);
    }
}

void NetIOMPDealerRouter::initRouter() {
    // Setup the ROUTER (server) socket
    int linger = 0;
    m_routerSocket.set(zmq::sockopt::linger, linger);
    // Bind to our Party's endpoint
    auto [myIp, myPort] = m_partyInfo.at(m_partyId);
    std::string bindEndpoint = "tcp://" + myIp + ":" + std::to_string(myPort);
    std::cout << "Party " << m_partyId << " binding to " << bindEndpoint << "\n";
    m_routerSocket.bind(bindEndpoint);
}

void NetIOMPDealerRouter::initDealers() {
    // Setup DEALER (client) sockets for all other parties
    int linger = 0;
    for (const auto& [pid, ipPort] : m_partyInfo) {
        if (pid == m_partyId || pid > m_totalParties)
            continue;

        auto [ip, port] = ipPort;
        std::string connectEndpoint = "tcp://" + ip + ":" + std::to_string(port);

        auto dealerSocket = std::make_unique<zmq::socket_t>(m_context, ZMQ_DEALER);
        dealerSocket->set(zmq::sockopt::linger, linger);

        // Set a unique identity for the DEALER socket by including target party ID
        std::string identity = getIdentity(m_partyId) + "_to_" + std::to_string(pid);
        dealerSocket->set(zmq::sockopt::routing_id, identity);

        dealerSocket->connect(connectEndpoint);

        m_dealerSockets[pid] = std::move(dealerSocket);
    }
}

void NetIOMPDealerRouter::sendTo(PARTY_ID_T targetId, const void* data, LENGTH_T length)
{
    if (m_dealerSockets.find(targetId) == m_dealerSockets.end()) {
        throw std::runtime_error("[NetIOMPDealerRouter] Invalid targetId or socket not initialized.");
    }

    zmq::message_t dataMessage(length);
    std::memcpy(dataMessage.data(), data, length);

    m_dealerSockets[targetId]->send(dataMessage, zmq::send_flags::none);

    #ifdef ENABLE_COUT
    std::cout << "[NetIOMPDealerRouter] Sent message to Party " << targetId << "\n";
    #endif
}

void NetIOMPDealerRouter::sendToAll(const void* data, LENGTH_T length)
{
    for (const auto& [pid, sockPtr] : m_dealerSockets) {
        try {
            sendTo(pid, data, length);
            #if defined(ENABLE_COUT)
            std::cout << "[NetIOMPDealerRouter] Sent data to Party " << pid << "\n";
            #endif
        } catch (const std::exception& e) {
            std::cerr << "[NetIOMPDealerRouter] Failed to send to Party " << pid << ": " << e.what() << "\n";
        }
    }
}

size_t NetIOMPDealerRouter::receive(PARTY_ID_T& senderId, void* buffer, LENGTH_T maxLength)
{
    // 1) Attempt to receive routing ID frame with set timeout
    zmq::message_t routingIdMsg;
    auto idRes = m_routerSocket.recv(routingIdMsg, zmq::recv_flags::none);
    if (!idRes.has_value()) {
        // No message arrived within the timeout
        return 0;
    }

    // 2) Attempt to receive data frame
    zmq::message_t dataMsg;
    auto dataRes = m_routerSocket.recv(dataMsg, zmq::recv_flags::none);
    if (!dataRes.has_value()) {
        // No data arrived for the second frame
        return 0;
    }

    // Extract senderId from the routing ID
    std::string routingId = routingIdMsg.to_string();
    // Store routingId for use in reply(...)
    m_lastRoutingId = routingId;
    if (routingId.find("Party") != 0) {
        throw std::runtime_error("[NetIOMPDealerRouter] Invalid routing ID format.");
    }
    senderId = static_cast<PARTY_ID_T>(std::stoi(routingId.substr(5)));
    #if defined(ENABLE_COUT)
    std::cout << "[NetIOMPDealerRouter] Message received from Party " << senderId << "\n";
    #endif

    // Copy the data into the provided buffer
    size_t receivedLength = dataMsg.size();
    if (receivedLength > maxLength) {
        throw std::runtime_error("[NetIOMPDealerRouter] Buffer too small for received message.");
    }
    std::memcpy(buffer, dataMsg.data(), receivedLength);

    return receivedLength;
}

size_t NetIOMPDealerRouter::dealerReceive(PARTY_ID_T& routerId, void* buffer, LENGTH_T maxLength) {
   
    // Make sure the dealer socket is valid
    if (m_dealerSockets.find(routerId) == m_dealerSockets.end()) {
        throw std::runtime_error("[NetIOMPDealerRouter] Invalid routerId or socket not initialized.");
    }
    zmq::message_t msg;
    #if defined(ENABLE_COUT)
    std::cout << "[NetIOMPDealerRouter] Receiving from DEALER socket...\n";
    #endif
    auto res = m_dealerSockets[routerId]->recv(msg, zmq::recv_flags::none);
    if (!res.has_value()) {
        return 0;
    }
    // Copy the data into the provided buffer
    size_t receivedLength = msg.size();
    if (receivedLength > maxLength) {
        throw std::runtime_error("[NetIOMPDealerRouter] Buffer too small for received message.");
    }
    std::memcpy(buffer, msg.data(), receivedLength);
    return receivedLength;
}

void NetIOMPDealerRouter::reply(const void* data, LENGTH_T length)
{
    // Prepare routing ID message
    zmq::message_t routingIdMsg(m_lastRoutingId.size());
    std::memcpy(routingIdMsg.data(), m_lastRoutingId.data(), m_lastRoutingId.size());

    // Prepare reply message
    zmq::message_t replyMsg(length);
    std::memcpy(replyMsg.data(), data, length);

    // Send multipart reply: [routing ID][reply data]
    m_routerSocket.send(routingIdMsg, zmq::send_flags::sndmore);
    m_routerSocket.send(replyMsg, zmq::send_flags::none);
}

void NetIOMPDealerRouter::reply(void* routingIdMsg, const void* data, LENGTH_T length)
{
    const char* routingIdCharPtr = static_cast<const char*>(routingIdMsg);
    std::string routingIdStr(routingIdCharPtr, m_lastRoutingId.size());
    std::cout << "[NetIOMPDealerRouter] Received routing ID: " << routingIdStr << "\n";

    zmq::message_t routingId(routingIdStr.size());
    std::memcpy(routingId.data(), routingIdStr.data(), routingIdStr.size());

    zmq::message_t reply(length);
    std::memcpy(reply.data(), data, length);

    m_routerSocket.send(routingId, zmq::send_flags::sndmore);
    m_routerSocket.send(reply, zmq::send_flags::none);
}

void NetIOMPDealerRouter::reply(void* routingIdMsg, LENGTH_T size, const void* data, LENGTH_T length)
{
    const char* routingIdCharPtr = static_cast<const char*>(routingIdMsg);
    std::string routingIdStr(routingIdCharPtr, size);
    std::cout << "[NetIOMPDealerRouter] Received routing ID: " << routingIdStr << "\n";

    zmq::message_t routingId(size);
    std::memcpy(routingId.data(), routingIdStr.data(), size);

    zmq::message_t replyMsg(length);
    std::memcpy(replyMsg.data(), data, length);

    m_routerSocket.send(routingId, zmq::send_flags::sndmore);
    m_routerSocket.send(replyMsg, zmq::send_flags::none);
}

std::string NetIOMPDealerRouter::getLastRoutingId() const
{
    return m_lastRoutingId;
}

void NetIOMPDealerRouter::close()
{
    // Set linger to 0 to prevent hanging on close
    int linger = 0;

    if (m_routerSocket) {
        m_routerSocket.set(zmq::sockopt::linger, linger);
        m_routerSocket.close();
        #ifdef ENABLE_COUT
        std::cout << "[NetIOMPDealerRouter] Closed ROUTER socket.\n";
        #endif
    }

    for (auto& [pid, sockPtr] : m_dealerSockets) {
        if (sockPtr) {
            sockPtr->set(zmq::sockopt::linger, linger);
            sockPtr->close();
            #ifdef ENABLE_COUT
            std::cout << "[NetIOMPDealerRouter] Closed DEALER socket for Party " << pid << ".\n";
            #endif
        }
    }

    m_context.close();
}

NetIOMPDealerRouter::~NetIOMPDealerRouter()
{
    close();
}