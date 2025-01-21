#include "NetIOMPDealerRouter.h"
#include <cstring>
#include <iostream>
#include <thread>

NetIOMPDealerRouter::NetIOMPDealerRouter(PARTY_ID_T partyId,
                                         const std::map<PARTY_ID_T, std::pair<std::string, int>>& partyInfo)
    : m_partyId(partyId), m_partyInfo(partyInfo), m_context(1)
{
    if (m_partyInfo.find(m_partyId) == m_partyInfo.end()) {
        throw std::runtime_error("[NetIOMPDealerRouter] Party ID not found in partyInfo map.");
    }
}

std::string NetIOMPDealerRouter::getIdentity(PARTY_ID_T partyId)
{
    return "Party" + std::to_string(partyId);
}

void NetIOMPDealerRouter::init()
{
    // Setup the ROUTER (server) socket
    m_routerSocket = std::make_unique<zmq::socket_t>(m_context, ZMQ_ROUTER);

    // Set socket options
    int linger = 0;
    m_routerSocket->set(zmq::sockopt::linger, linger);

    auto [myIp, myPort] = m_partyInfo.at(m_partyId);
    std::string bindEndpoint = "tcp://" + myIp + ":" + std::to_string(myPort);

    std::cout << "Party " << m_partyId << " binding to " << bindEndpoint << "\n";
    m_routerSocket->bind(bindEndpoint);

    // Setup DEALER (client) sockets for all other parties
    for (const auto& [pid, ipPort] : m_partyInfo) {
        if (pid == m_partyId)
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

    // Removed forced waiting for reply and retry logic
    m_dealerSockets[targetId]->send(dataMessage, zmq::send_flags::none);
}

void NetIOMPDealerRouter::sendToAll(const void* data, LENGTH_T length)
{
    for (const auto& [pid, sockPtr] : m_dealerSockets) {
        try {
            sendTo(pid, data, length);
            std::cout << "[NetIOMPDealerRouter] Sent data to Party " << pid << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[NetIOMPDealerRouter] Failed to send to Party " << pid << ": " << e.what() << "\n";
        }
    }
}

size_t NetIOMPDealerRouter::receive(PARTY_ID_T& senderId, void* buffer, LENGTH_T maxLength)
{
    // 1) Receive routing ID frame
    zmq::message_t routingIdMsg;
    auto idRes = m_routerSocket->recv(routingIdMsg, zmq::recv_flags::none);
    if (!idRes) {
        throw std::runtime_error("[NetIOMPDealerRouter] Failed to receive routing ID.");
    }

    // 2) Receive data frame
    zmq::message_t dataMsg;
    auto dataRes = m_routerSocket->recv(dataMsg, zmq::recv_flags::none);
    if (!dataRes) {
        throw std::runtime_error("[NetIOMPDealerRouter] Failed to receive data frame.");
    }

    // Extract senderId from the routing ID
    std::string routingId = routingIdMsg.to_string();
    if (routingId.find("Party") != 0) {
        throw std::runtime_error("[NetIOMPDealerRouter] Invalid routing ID format.");
    }
    senderId = static_cast<PARTY_ID_T>(std::stoi(routingId.substr(5)));

    std::cout << "[NetIOMPDealerRouter] Message received from Party " << senderId << "\n";

    // Copy the data into the provided buffer
    size_t receivedLength = dataMsg.size();
    if (receivedLength > maxLength) {
        throw std::runtime_error("[NetIOMPDealerRouter] Buffer too small for received message.");
    }
    std::memcpy(buffer, dataMsg.data(), receivedLength);

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
    m_routerSocket->send(routingIdMsg, zmq::send_flags::sndmore);
    m_routerSocket->send(replyMsg, zmq::send_flags::none);
}

void NetIOMPDealerRouter::close()
{
    // Set linger to 0 to prevent hanging on close
    int linger = 0;

    if (m_routerSocket) {
        m_routerSocket->set(zmq::sockopt::linger, linger);
        m_routerSocket->close();
        m_routerSocket.reset();
    }

    for (auto& [pid, sockPtr] : m_dealerSockets) {
        if (sockPtr) {
            sockPtr->set(zmq::sockopt::linger, linger);
            sockPtr->close();
            sockPtr.reset();
        }
    }

    m_context.close();
}

NetIOMPDealerRouter::~NetIOMPDealerRouter()
{
    close();
}