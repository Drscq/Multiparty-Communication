#ifndef IMPC_COMMUNICATION_H
#define IMPC_COMMUNICATION_H

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <chrono>

namespace mpc {

// Forward declarations
class Message;
class MessageBuilder;

/**
 * @brief Core interface for MPC party communication
 * 
 * This interface defines the contract for multi-party computation communication
 * implementations. It provides asynchronous and synchronous communication patterns
 * suitable for MPC protocols.
 */
class IMPCCommunication {
public:
    using PartyId = uint32_t;
    using MessageHandler = std::function<void(PartyId, const Message&)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    using TimeoutDuration = std::chrono::milliseconds;

    virtual ~IMPCCommunication() = default;

    /**
     * @brief Initialize the communication layer
     * @throws std::runtime_error if initialization fails
     */
    virtual void initialize() = 0;

    /**
     * @brief Shutdown the communication layer gracefully
     */
    virtual void shutdown() = 0;

    /**
     * @brief Send a message to a specific party
     * @param targetId The recipient party ID
     * @param message The message to send
     * @return true if message was queued successfully
     */
    virtual bool send(PartyId targetId, const Message& message) = 0;

    /**
     * @brief Broadcast a message to all other parties
     * @param message The message to broadcast
     * @return true if message was queued successfully
     */
    virtual bool broadcast(const Message& message) = 0;

    /**
     * @brief Send a message to multiple specific parties
     * @param targetIds Vector of recipient party IDs
     * @param message The message to send
     * @return true if message was queued successfully
     */
    virtual bool multicast(const std::vector<PartyId>& targetIds, const Message& message) = 0;

    /**
     * @brief Receive a message (blocking with timeout)
     * @param senderId Output parameter for sender's ID
     * @param message Output parameter for received message
     * @param timeout Maximum time to wait
     * @return true if message was received, false on timeout
     */
    virtual bool receive(PartyId& senderId, Message& message, 
                        TimeoutDuration timeout = TimeoutDuration(5000)) = 0;

    /**
     * @brief Set asynchronous message handler
     * @param handler Function to call when messages arrive
     */
    virtual void setMessageHandler(MessageHandler handler) = 0;

    /**
     * @brief Set error handler for asynchronous errors
     * @param handler Function to call on errors
     */
    virtual void setErrorHandler(ErrorHandler handler) = 0;

    /**
     * @brief Check if communication layer is ready
     * @return true if initialized and ready
     */
    virtual bool isReady() const = 0;

    /**
     * @brief Get this party's ID
     * @return Party ID
     */
    virtual PartyId getPartyId() const = 0;

    /**
     * @brief Get total number of parties in the computation
     * @return Number of parties
     */
    virtual size_t getTotalParties() const = 0;

    /**
     * @brief Get statistics about communication performance
     * @return JSON string with statistics
     */
    virtual std::string getStatistics() const = 0;
};

/**
 * @brief Message class for MPC communication
 */
class Message {
public:
    enum class Type : uint8_t {
        DATA = 0,
        CONTROL = 1,
        SYNC = 2,
        SHARE = 3,
        TRIPLE = 4,
        PARTIAL_OPEN = 5,
        CUSTOM = 255
    };

    Message() = default;
    Message(Type type, const std::vector<uint8_t>& payload);
    Message(Type type, std::vector<uint8_t>&& payload);

    Type getType() const { return type_; }
    const std::vector<uint8_t>& getPayload() const { return payload_; }
    std::vector<uint8_t>& getPayload() { return payload_; }
    
    size_t size() const { return payload_.size(); }
    bool empty() const { return payload_.empty(); }

    // Serialization helpers
    std::vector<uint8_t> serialize() const;
    static Message deserialize(const std::vector<uint8_t>& data);

private:
    Type type_ = Type::DATA;
    std::vector<uint8_t> payload_;
};

/**
 * @brief Builder pattern for constructing messages
 */
class MessageBuilder {
public:
    MessageBuilder& setType(Message::Type type);
    MessageBuilder& addData(const void* data, size_t length);
    MessageBuilder& addString(const std::string& str);
    MessageBuilder& addUint32(uint32_t value);
    MessageBuilder& addUint64(uint64_t value);
    Message build();

private:
    Message::Type type_ = Message::Type::DATA;
    std::vector<uint8_t> buffer_;
};

} // namespace mpc

#endif // IMPC_COMMUNICATION_H
