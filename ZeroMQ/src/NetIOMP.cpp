#include "NetIOMP.h"
#include <cstring> // For memcpy
#include <iostream>

// Constructor
NetIOMP::NetIOMP(int partyId, const std::map<int, std::pair<std::string, int>>& partyInfo)
    : m_partyId(partyId), m_partyInfo(partyInfo), m_context(1)
{
    if (m_partyInfo.find(m_partyId) == m_partyInfo.end()) {
        throw std::runtime_error("[NetIOMP] Party ID not found in partyInfo map.");
    }
}

// Initialize sockets
void NetIOMP::init()
{
    // Setup the REP (server) socket
    m_repSocket = std::make_unique<zmq::socket_t>(m_context, ZMQ_REP);
    auto [myIp, myPort] = m_partyInfo.at(m_partyId);
    std::string bindEndpoint = "tcp://" + myIp + ":" + std::to_string(myPort);
    m_repSocket->bind(bindEndpoint);

    // Setup REQ (client) sockets for all other parties
    for (const auto& [pid, ipPort] : m_partyInfo) {
        if (pid == m_partyId)
            continue;

        auto [ip, port] = ipPort;
        std::string connectEndpoint = "tcp://" + ip + ":" + std::to_string(port);

        auto reqSocket = std::make_unique<zmq::socket_t>(m_context, ZMQ_REQ);
        reqSocket->connect(connectEndpoint);

        m_reqSockets[pid] = std::move(reqSocket);
    }
}

void NetIOMP::sendTo(int targetId, const void* data, size_t length)
{
    if (m_reqSockets.find(targetId) == m_reqSockets.end()) {
        throw std::runtime_error("[NetIOMP] Invalid targetId or socket not initialized.");
    }

    // Create multipart message with sender ID and data
    zmq::message_t idMessage(sizeof(int));
    std::memcpy(idMessage.data(), &m_partyId, sizeof(int));

    zmq::message_t dataMessage(length);
    std::memcpy(dataMessage.data(), data, length);

    // Send both parts
    m_reqSockets[targetId]->send(idMessage, zmq::send_flags::sndmore);
    m_reqSockets[targetId]->send(dataMessage, zmq::send_flags::none);

    // Wait for a reply (REQ/REP requires a round-trip)
    zmq::message_t reply;
    auto result = m_reqSockets[targetId]->recv(reply, zmq::recv_flags::none);

    if (!result) {
        throw std::runtime_error("[NetIOMP] Failed to receive reply from target.");
    }

    // Optionally handle the reply (e.g., log it, process it).
}


size_t NetIOMP::receive(int& senderId, void* buffer, size_t maxLength)
{
    // Receive the multipart message
    zmq::message_t idMessage;
    zmq::message_t dataMessage;

    auto idResult = m_repSocket->recv(idMessage, zmq::recv_flags::none);
    if (!idResult) {
        throw std::runtime_error("[NetIOMP] Failed to receive sender ID.");
    }

    auto dataResult = m_repSocket->recv(dataMessage, zmq::recv_flags::none);
    if (!dataResult) {
        throw std::runtime_error("[NetIOMP] Failed to receive data message.");
    }

    // Extract the sender ID
    if (idMessage.size() != sizeof(int)) {
        throw std::runtime_error("[NetIOMP] Received invalid sender ID size.");
    }
    std::memcpy(&senderId, idMessage.data(), sizeof(int));

    // Extract the data
    size_t receivedLength = dataMessage.size();
    if (receivedLength > maxLength) {
        throw std::runtime_error("[NetIOMP] Buffer too small for received message.");
    }

    std::memcpy(buffer, dataMessage.data(), receivedLength);
    return receivedLength;
}


// Reply with binary data
void NetIOMP::reply(const void* data, size_t length)
{
    zmq::message_t reply(length);
    std::memcpy(reply.data(), data, length);

    m_repSocket->send(reply, zmq::send_flags::none);
}

// Close all sockets
void NetIOMP::close()
{
    if (m_repSocket) {
        m_repSocket->close();
        m_repSocket.reset();
    }

    for (auto& [pid, sockPtr] : m_reqSockets) {
        if (sockPtr) {
            sockPtr->close();
            sockPtr.reset();
        }
    }
}

// Destructor
NetIOMP::~NetIOMP()
{
    close();
}

bool NetIOMP::pollAndProcess(int timeout)
{
    zmq::pollitem_t items[] = {
        { static_cast<void*>(m_repSocket->handle()), 0, ZMQ_POLLIN, 0 }
    };

    zmq::poll(&items[0], 1, std::chrono::milliseconds(timeout));

    if (items[0].revents & ZMQ_POLLIN) {
        int senderId;
        char buffer[256];
        size_t receivedSize = receive(senderId, buffer, sizeof(buffer));
        buffer[receivedSize] = '\0'; // Null-terminate for printing

        std::cout << "Party " << m_partyId << " received: '" << buffer << "' from Party " << senderId << std::endl;

        // Reply to the sender
        std::string reply = "Hello back from Party " + std::to_string(m_partyId);
        std::cout << "Party " << m_partyId << " replying: '" << reply << "' to Party " << senderId << std::endl;
        this->reply(reply.c_str(), reply.size());

        return true; // A message was received and processed
    }

    return false; // No message arrived this poll cycle
}
