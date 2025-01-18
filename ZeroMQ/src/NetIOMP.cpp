#include "NetIOMP.h"
#include "config.h"
#include <cstring> // For memcpy
#include <iostream>
#include <thread> // For std::this_thread::sleep_for

// Constructor
NetIOMP::NetIOMP(PARTY_ID_T partyId,
                 const std::map<PARTY_ID_T, std::pair<std::string, int>>& partyInfo)
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
    
    // Set socket options
    int linger = 0;
    m_repSocket->set(zmq::sockopt::linger, linger);
    
    auto [myIp, myPort] = m_partyInfo.at(m_partyId);
    std::string bindEndpoint = "tcp://" + myIp + ":" + std::to_string(myPort);
    
    std::cout << "Party " << m_partyId << " binding to " << bindEndpoint << "\n";
    m_repSocket->bind(bindEndpoint);

    // Setup REQ (client) sockets for all other parties
    for (const auto& [pid, ipPort] : m_partyInfo) {
        if (pid == m_partyId)
            continue;

        auto [ip, port] = ipPort;
        std::string connectEndpoint = "tcp://" + ip + ":" + std::to_string(port);

        auto reqSocket = std::make_unique<zmq::socket_t>(m_context, ZMQ_REQ);
        reqSocket->set(zmq::sockopt::linger, linger);
        reqSocket->connect(connectEndpoint);

        m_reqSockets[pid] = std::move(reqSocket);
    }
}

void NetIOMP::sendTo(PARTY_ID_T targetId, const void* data, LENGTH_T length)
{
    if (m_reqSockets.find(targetId) == m_reqSockets.end()) {
        throw std::runtime_error("[NetIOMP] Invalid targetId or socket not initialized.");
    }

    // Create multipart message with sender ID and data
    zmq::message_t idMessage(sizeof(PARTY_ID_T));
    std::memcpy(idMessage.data(), &m_partyId, sizeof(PARTY_ID_T));

    zmq::message_t dataMessage(length);
    std::memcpy(dataMessage.data(), data, length);

    // Retry mechanism
    const int maxRetries = 5;
    int retries = 0;
    while (retries < maxRetries) {
        try {
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
            break; // Exit the retry loop if successful
        } catch (const zmq::error_t& e) {
            std::cerr << "[NetIOMP] Error sending to target " << targetId << ": " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait before retrying
            retries++;
        }
    }

    if (retries == maxRetries) {
        throw std::runtime_error("[NetIOMP] Max retries reached. Failed to send message.");
    }
}

size_t NetIOMP::receive(PARTY_ID_T& senderId, void* buffer, LENGTH_T maxLength)
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
    if (idMessage.size() != sizeof(PARTY_ID_T)) {
        throw std::runtime_error("[NetIOMP] Received invalid sender ID size.");
    }
    std::memcpy(&senderId, idMessage.data(), sizeof(PARTY_ID_T));

    // Extract the data
    size_t receivedLength = dataMessage.size();
    if (receivedLength > maxLength) {
        throw std::runtime_error("[NetIOMP] Buffer too small for received message.");
    }

    std::memcpy(buffer, dataMessage.data(), receivedLength);
    return receivedLength;
}


// Reply with binary data
void NetIOMP::reply(const void* data, LENGTH_T length)
{
    zmq::message_t reply(length);
    std::memcpy(reply.data(), data, length);

    m_repSocket->send(reply, zmq::send_flags::none);
}

// Close all sockets
void NetIOMP::close()
{
    // Set linger to 0 to prevent hanging on close
    int linger = 0;
    
    if (m_repSocket) {
        m_repSocket->set(zmq::sockopt::linger, linger);
        m_repSocket->close();
        m_repSocket.reset();
    }

    for (auto& [pid, sockPtr] : m_reqSockets) {
        if (sockPtr) {
            sockPtr->set(zmq::sockopt::linger, linger);
            sockPtr->close();
            sockPtr.reset();
        }
    }
    
    m_context.close();
}

// Destructor
NetIOMP::~NetIOMP()
{
    close();
}

// Remove pollAndProcess and runServer methods as they're no longer needed
