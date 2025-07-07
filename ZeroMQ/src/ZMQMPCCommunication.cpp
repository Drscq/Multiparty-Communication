#include "ZMQMPCCommunication.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace mpc {

ZMQMPCCommunication::ZMQMPCCommunication(PartyId partyId, const std::map<PartyId, std::string>& partyEndpoints)
    : partyId_(partyId), partyEndpoints_(partyEndpoints), context_(1) {
    routerSocket_ = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::router);
}

ZMQMPCCommunication::~ZMQMPCCommunication() {
    if (isRunning_) {
        shutdown();
    }
}

void ZMQMPCCommunication::initialize() {
    if (isReady_) {
        throw std::runtime_error("Communication already initialized");
    }

    try {
        // Configure router socket
        routerSocket_->set(zmq::sockopt::rcvtimeo, 100); // 100ms timeout for polling
        routerSocket_->set(zmq::sockopt::linger, 0);
        
        // Bind router socket to this party's endpoint
        auto it = partyEndpoints_.find(partyId_);
        if (it == partyEndpoints_.end()) {
            throw std::runtime_error("Party endpoint not found for party " + std::to_string(partyId_));
        }
        
        routerSocket_->bind(it->second);
        logMessage("Router socket bound to " + it->second);
        
        // Create dealer sockets for each remote party
        for (const auto& [pid, endpoint] : partyEndpoints_) {
            if (pid != partyId_) {
                auto dealerSocket = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::dealer);
                
                // Set identity for the dealer socket
                std::string identity = createIdentity(partyId_);
                dealerSocket->set(zmq::sockopt::routing_id, identity);
                dealerSocket->set(zmq::sockopt::linger, 0);
                
                // Connect to remote party
                dealerSocket->connect(endpoint);
                dealerSockets_[pid] = std::move(dealerSocket);
                
                logMessage("Dealer socket connected to party " + std::to_string(pid) + " at " + endpoint);
            }
        }
        
        // Start receiver thread
        isRunning_ = true;
        receiverThread_ = std::make_unique<std::thread>(&ZMQMPCCommunication::receiverLoop, this);
        
        isReady_ = true;
        logMessage("Communication initialized for party " + std::to_string(partyId_));
        
    } catch (const zmq::error_t& e) {
        reportError("ZMQ error during initialization: " + std::string(e.what()));
        throw;
    }
}

void ZMQMPCCommunication::shutdown() {
    if (!isReady_) {
        return;
    }
    
    logMessage("Shutting down communication");
    
    // Stop receiver thread
    isRunning_ = false;
    queueCV_.notify_all();
    
    if (receiverThread_ && receiverThread_->joinable()) {
        receiverThread_->join();
    }
    
    // Close all sockets
    for (auto& [pid, socket] : dealerSockets_) {
        if (socket) {
            socket->close();
        }
    }
    dealerSockets_.clear();
    
    if (routerSocket_) {
        routerSocket_->close();
    }
    
    context_.close();
    isReady_ = false;
    
    logMessage("Communication shutdown complete");
}

bool ZMQMPCCommunication::send(PartyId targetId, const Message& message) {
    if (!isReady_) {
        reportError("Cannot send: communication not initialized");
        return false;
    }
    
    auto it = dealerSockets_.find(targetId);
    if (it == dealerSockets_.end()) {
        reportError("Cannot send: invalid target party " + std::to_string(targetId));
        return false;
    }
    
    try {
        auto serialized = message.serialize();
        zmq::message_t zmqMsg(serialized.data(), serialized.size());
        
        it->second->send(zmqMsg, zmq::send_flags::none);
        
        messagesSent_++;
        bytessSent_ += serialized.size();
        
        logMessage("Sent message to party " + std::to_string(targetId) + 
                  " (type: " + std::to_string(static_cast<int>(message.getType())) + 
                  ", size: " + std::to_string(serialized.size()) + ")");
        
        return true;
        
    } catch (const zmq::error_t& e) {
        reportError("ZMQ error while sending: " + std::string(e.what()));
        return false;
    }
}

bool ZMQMPCCommunication::broadcast(const Message& message) {
    bool allSuccess = true;
    
    for (const auto& [pid, _] : partyEndpoints_) {
        if (pid != partyId_) {
            if (!send(pid, message)) {
                allSuccess = false;
            }
        }
    }
    
    return allSuccess;
}

bool ZMQMPCCommunication::multicast(const std::vector<PartyId>& targetIds, const Message& message) {
    bool allSuccess = true;
    
    for (PartyId targetId : targetIds) {
        if (targetId != partyId_ && !send(targetId, message)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool ZMQMPCCommunication::receive(PartyId& senderId, Message& message, TimeoutDuration timeout) {
    if (!isReady_) {
        reportError("Cannot receive: communication not initialized");
        return false;
    }
    
    std::unique_lock<std::mutex> lock(queueMutex_);
    
    // Wait for message with timeout
    bool messageAvailable = queueCV_.wait_for(lock, timeout, [this] {
        return !messageQueue_.empty() || !isRunning_;
    });
    
    if (!messageAvailable || messageQueue_.empty()) {
        return false;
    }
    
    // Get message from queue
    auto [sid, msg] = std::move(messageQueue_.front());
    messageQueue_.pop();
    
    senderId = sid;
    message = std::move(msg);
    
    return true;
}

void ZMQMPCCommunication::setMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

void ZMQMPCCommunication::setErrorHandler(ErrorHandler handler) {
    errorHandler_ = std::move(handler);
}

bool ZMQMPCCommunication::isReady() const {
    return isReady_;
}

ZMQMPCCommunication::PartyId ZMQMPCCommunication::getPartyId() const {
    return partyId_;
}

size_t ZMQMPCCommunication::getTotalParties() const {
    return partyEndpoints_.size();
}

std::string ZMQMPCCommunication::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    std::stringstream ss;
    ss << "{"
       << "\"party_id\": " << partyId_ << ","
       << "\"messages_sent\": " << messagesSent_ << ","
       << "\"messages_received\": " << messagesReceived_ << ","
       << "\"bytes_sent\": " << bytessSent_ << ","
       << "\"bytes_received\": " << bytesReceived_ << ","
       << "\"queued_messages\": " << getQueuedMessageCount()
       << "}";
    
    return ss.str();
}

size_t ZMQMPCCommunication::getQueuedMessageCount() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return messageQueue_.size();
}

void ZMQMPCCommunication::receiverLoop() {
    logMessage("Receiver thread started");
    
    while (isRunning_) {
        try {
            zmq::message_t identityMsg;
            auto result = routerSocket_->recv(identityMsg, zmq::recv_flags::none);
            
            if (!result.has_value()) {
                continue; // Timeout, check if still running
            }
            
            // Extract sender ID from identity
            std::string identity = identityMsg.to_string();
            PartyId senderId = extractPartyId(identity);
            
            // Receive the actual message
            zmq::message_t dataMsg;
            result = routerSocket_->recv(dataMsg, zmq::recv_flags::none);
            
            if (!result.has_value()) {
                reportError("Failed to receive data frame after identity frame");
                continue;
            }
            
            // Deserialize message
            std::vector<uint8_t> data(static_cast<uint8_t*>(dataMsg.data()), 
                                     static_cast<uint8_t*>(dataMsg.data()) + dataMsg.size());
            Message message = Message::deserialize(data);
            
            messagesReceived_++;
            bytesReceived_ += data.size();
            
            logMessage("Received message from party " + std::to_string(senderId) + 
                      " (type: " + std::to_string(static_cast<int>(message.getType())) + 
                      ", size: " + std::to_string(data.size()) + ")");
            
            // Process the message
            processReceivedMessage(senderId, message);
            
        } catch (const zmq::error_t& e) {
            if (isRunning_) {
                reportError("ZMQ error in receiver loop: " + std::string(e.what()));
            }
        } catch (const std::exception& e) {
            reportError("Error in receiver loop: " + std::string(e.what()));
        }
    }
    
    logMessage("Receiver thread stopped");
}

void ZMQMPCCommunication::processReceivedMessage(PartyId senderId, const Message& message) {
    // If async handler is set, use it
    if (messageHandler_) {
        try {
            messageHandler_(senderId, message);
        } catch (const std::exception& e) {
            reportError("Error in message handler: " + std::string(e.what()));
        }
    } else {
        // Otherwise, queue the message for synchronous receive
        std::lock_guard<std::mutex> lock(queueMutex_);
        messageQueue_.emplace(senderId, message);
        queueCV_.notify_one();
    }
}

std::string ZMQMPCCommunication::createIdentity(PartyId pid) const {
    return "Party" + std::to_string(pid);
}

ZMQMPCCommunication::PartyId ZMQMPCCommunication::extractPartyId(const std::string& identity) const {
    if (identity.find("Party") != 0) {
        throw std::runtime_error("Invalid identity format: " + identity);
    }
    
    return static_cast<PartyId>(std::stoul(identity.substr(5)));
}

void ZMQMPCCommunication::logMessage(const std::string& message) const {
    if (loggingEnabled_) {
        std::cout << "[Party " << partyId_ << "] " << message << std::endl;
    }
}

void ZMQMPCCommunication::reportError(const std::string& error) {
    if (errorHandler_) {
        errorHandler_(error);
    } else {
        std::cerr << "[Party " << partyId_ << "] ERROR: " << error << std::endl;
    }
}

// Factory function implementation
std::unique_ptr<IMPCCommunication> createZMQMPCCommunication(
    IMPCCommunication::PartyId partyId,
    const std::map<IMPCCommunication::PartyId, std::string>& partyEndpoints) {
    return std::make_unique<ZMQMPCCommunication>(partyId, partyEndpoints);
}

} // namespace mpc
