#ifndef ZMQ_MPC_COMMUNICATION_H
#define ZMQ_MPC_COMMUNICATION_H

#include "IMPCCommunication.h"
#include <zmq.hpp>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

namespace mpc {

/**
 * @brief ZeroMQ-based implementation of MPC communication using Router-Dealer pattern
 * 
 * This class provides reliable multi-party communication for MPC protocols using
 * ZeroMQ's router-dealer pattern. Each party has:
 * - One ROUTER socket: Receives messages from other parties
 * - Multiple DEALER sockets: One per remote party for sending messages
 */
class ZMQMPCCommunication : public IMPCCommunication {
public:
    /**
     * @brief Constructor
     * @param partyId This party's unique identifier
     * @param partyEndpoints Map of party IDs to their ZMQ endpoints (e.g., "tcp://127.0.0.1:5555")
     */
    ZMQMPCCommunication(PartyId partyId, const std::map<PartyId, std::string>& partyEndpoints);
    
    ~ZMQMPCCommunication() override;

    // IMPCCommunication interface implementation
    void initialize() override;
    void shutdown() override;
    bool send(PartyId targetId, const Message& message) override;
    bool broadcast(const Message& message) override;
    bool multicast(const std::vector<PartyId>& targetIds, const Message& message) override;
    bool receive(PartyId& senderId, Message& message, TimeoutDuration timeout = TimeoutDuration(5000)) override;
    void setMessageHandler(MessageHandler handler) override;
    void setErrorHandler(ErrorHandler handler) override;
    bool isReady() const override;
    PartyId getPartyId() const override;
    size_t getTotalParties() const override;
    std::string getStatistics() const override;

    // Additional methods for testing and debugging
    void enableLogging(bool enable) { loggingEnabled_ = enable; }
    size_t getQueuedMessageCount() const;
    
private:
    // Configuration
    PartyId partyId_;
    std::map<PartyId, std::string> partyEndpoints_;
    
    // ZeroMQ components
    zmq::context_t context_;
    std::unique_ptr<zmq::socket_t> routerSocket_;
    std::unordered_map<PartyId, std::unique_ptr<zmq::socket_t>> dealerSockets_;
    
    // Threading
    std::unique_ptr<std::thread> receiverThread_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isReady_{false};
    
    // Message queue for async handling
    std::queue<std::pair<PartyId, Message>> messageQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCV_;
    
    // Handlers
    MessageHandler messageHandler_;
    ErrorHandler errorHandler_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    std::atomic<uint64_t> messagesSent_{0};
    std::atomic<uint64_t> messagesReceived_{0};
    std::atomic<uint64_t> bytessSent_{0};
    std::atomic<uint64_t> bytesReceived_{0};
    
    // Configuration options
    bool loggingEnabled_{false};
    
    // Private methods
    void receiverLoop();
    void processReceivedMessage(PartyId senderId, const Message& message);
    std::string createIdentity(PartyId pid) const;
    PartyId extractPartyId(const std::string& identity) const;
    void logMessage(const std::string& message) const;
    void reportError(const std::string& error);
};

/**
 * @brief Factory function to create ZMQ MPC communication instances
 */
std::unique_ptr<IMPCCommunication> createZMQMPCCommunication(
    IMPCCommunication::PartyId partyId,
    const std::map<IMPCCommunication::PartyId, std::string>& partyEndpoints);

} // namespace mpc

#endif // ZMQ_MPC_COMMUNICATION_H
