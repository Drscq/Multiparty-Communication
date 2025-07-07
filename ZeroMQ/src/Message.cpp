#include "IMPCCommunication.h"
#include <cstring>
#include <stdexcept>

namespace mpc {

Message::Message(Type type, const std::vector<uint8_t>& payload)
    : type_(type), payload_(payload) {}

Message::Message(Type type, std::vector<uint8_t>&& payload)
    : type_(type), payload_(std::move(payload)) {}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(sizeof(type_) + sizeof(uint32_t) + payload_.size());
    
    // Add type
    result.push_back(static_cast<uint8_t>(type_));
    
    // Add payload size (4 bytes, little-endian)
    uint32_t size = static_cast<uint32_t>(payload_.size());
    result.push_back(size & 0xFF);
    result.push_back((size >> 8) & 0xFF);
    result.push_back((size >> 16) & 0xFF);
    result.push_back((size >> 24) & 0xFF);
    
    // Add payload
    result.insert(result.end(), payload_.begin(), payload_.end());
    
    return result;
}

Message Message::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 5) {
        throw std::runtime_error("Message too small to deserialize");
    }
    
    // Extract type
    Type type = static_cast<Type>(data[0]);
    
    // Extract payload size
    uint32_t size = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
    
    if (data.size() < 5 + size) {
        throw std::runtime_error("Message payload incomplete");
    }
    
    // Extract payload
    std::vector<uint8_t> payload(data.begin() + 5, data.begin() + 5 + size);
    
    return Message(type, std::move(payload));
}


// MessageBuilder implementation
MessageBuilder& MessageBuilder::setType(Message::Type type) {
    type_ = type;
    return *this;
}

MessageBuilder& MessageBuilder::addData(const void* data, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    buffer_.insert(buffer_.end(), bytes, bytes + length);
    return *this;
}

MessageBuilder& MessageBuilder::addString(const std::string& str) {
    // Add string length first
    addUint32(static_cast<uint32_t>(str.size()));
    // Then add string data
    addData(str.data(), str.size());
    return *this;
}

MessageBuilder& MessageBuilder::addUint32(uint32_t value) {
    buffer_.push_back(value & 0xFF);
    buffer_.push_back((value >> 8) & 0xFF);
    buffer_.push_back((value >> 16) & 0xFF);
    buffer_.push_back((value >> 24) & 0xFF);
    return *this;
}

MessageBuilder& MessageBuilder::addUint64(uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        buffer_.push_back((value >> (i * 8)) & 0xFF);
    }
    return *this;
}

Message MessageBuilder::build() {
    return Message(type_, std::move(buffer_));
}

} // namespace mpc
