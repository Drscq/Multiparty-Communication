#include <gtest/gtest.h>
#include "../src/ZMQMPCCommunication.h"
#include "../src/Message.cpp"
#include "../src/ZMQMPCCommunication.cpp"
#include <thread>
#include <chrono>
#include <atomic>
#include <random>

using namespace mpc;
using namespace std::chrono_literals;

class ZMQMPCCommunicationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup party endpoints for testing
        basePort_ = 15000 + (std::rand() % 10000); // Random port to avoid conflicts
        
        for (uint32_t i = 1; i <= numParties_; ++i) {
            partyEndpoints_[i] = "tcp://127.0.0.1:" + std::to_string(basePort_ + i);
        }
    }
    
    void TearDown() override {
        // Ensure all communications are shut down
        for (auto& comm : communications_) {
            if (comm && comm->isReady()) {
                comm->shutdown();
            }
        }
        communications_.clear();
    }
    
    std::unique_ptr<IMPCCommunication> createPartyComm(uint32_t partyId) {
        auto comm = createZMQMPCCommunication(partyId, partyEndpoints_);
        return comm;
    }
    
    void initializeAllParties() {
        for (uint32_t i = 1; i <= numParties_; ++i) {
            auto comm = createPartyComm(i);
            comm->initialize();
            communications_.push_back(std::move(comm));
        }
        // Allow time for all connections to establish
        std::this_thread::sleep_for(100ms);
    }
    
protected:
    uint32_t numParties_ = 3;
    int basePort_;
    std::map<uint32_t, std::string> partyEndpoints_;
    std::vector<std::unique_ptr<IMPCCommunication>> communications_;
};

// Test 1: Basic initialization and shutdown
TEST_F(ZMQMPCCommunicationTest, InitializationAndShutdown) {
    auto comm = createPartyComm(1);
    
    EXPECT_FALSE(comm->isReady());
    EXPECT_EQ(comm->getPartyId(), 1u);
    EXPECT_EQ(comm->getTotalParties(), numParties_);
    
    comm->initialize();
    EXPECT_TRUE(comm->isReady());
    
    comm->shutdown();
    EXPECT_FALSE(comm->isReady());
}

// Test 2: Double initialization should throw
TEST_F(ZMQMPCCommunicationTest, DoubleInitializationThrows) {
    auto comm = createPartyComm(1);
    
    comm->initialize();
    EXPECT_THROW(comm->initialize(), std::runtime_error);
}

// Test 3: Send before initialization should fail
TEST_F(ZMQMPCCommunicationTest, SendBeforeInitializationFails) {
    auto comm = createPartyComm(1);
    
    Message msg(Message::Type::DATA, {1, 2, 3, 4});
    EXPECT_FALSE(comm->send(2, msg));
}

// Test 4: Point-to-point communication
TEST_F(ZMQMPCCommunicationTest, PointToPointCommunication) {
    initializeAllParties();
    
    // Party 1 sends to Party 2
    std::vector<uint8_t> testData = {1, 2, 3, 4, 5};
    Message sentMsg(Message::Type::DATA, testData);
    
    EXPECT_TRUE(communications_[0]->send(2, sentMsg));
    
    // Party 2 receives
    uint32_t senderId;
    Message receivedMsg;
    EXPECT_TRUE(communications_[1]->receive(senderId, receivedMsg, 1000ms));
    
    EXPECT_EQ(senderId, 1u);
    EXPECT_EQ(receivedMsg.getType(), Message::Type::DATA);
    EXPECT_EQ(receivedMsg.getPayload(), testData);
}

// Test 5: Broadcast communication
TEST_F(ZMQMPCCommunicationTest, BroadcastCommunication) {
    initializeAllParties();
    
    // Party 1 broadcasts
    std::vector<uint8_t> testData = {10, 20, 30};
    Message broadcastMsg(Message::Type::SHARE, testData);
    
    EXPECT_TRUE(communications_[0]->broadcast(broadcastMsg));
    
    // All other parties should receive
    for (size_t i = 1; i < numParties_; ++i) {
        uint32_t senderId;
        Message receivedMsg;
        EXPECT_TRUE(communications_[i]->receive(senderId, receivedMsg, 1000ms));
        
        EXPECT_EQ(senderId, 1u);
        EXPECT_EQ(receivedMsg.getType(), Message::Type::SHARE);
        EXPECT_EQ(receivedMsg.getPayload(), testData);
    }
}

// Test 6: Multicast communication
TEST_F(ZMQMPCCommunicationTest, MulticastCommunication) {
    initializeAllParties();
    
    // Party 1 multicasts to parties 2 and 3
    std::vector<uint8_t> testData = {100, 200};
    Message multicastMsg(Message::Type::CONTROL, testData);
    std::vector<uint32_t> targets = {2, 3};
    
    EXPECT_TRUE(communications_[0]->multicast(targets, multicastMsg));
    
    // Parties 2 and 3 should receive
    for (size_t i = 1; i < numParties_; ++i) {
        uint32_t senderId;
        Message receivedMsg;
        EXPECT_TRUE(communications_[i]->receive(senderId, receivedMsg, 1000ms));
        
        EXPECT_EQ(senderId, 1u);
        EXPECT_EQ(receivedMsg.getType(), Message::Type::CONTROL);
        EXPECT_EQ(receivedMsg.getPayload(), testData);
    }
}

// Test 7: Message serialization and deserialization
TEST_F(ZMQMPCCommunicationTest, MessageSerialization) {
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5, 6, 7, 8};
    Message original(Message::Type::TRIPLE, payload);
    
    auto serialized = original.serialize();
    auto deserialized = Message::deserialize(serialized);
    
    EXPECT_EQ(deserialized.getType(), original.getType());
    EXPECT_EQ(deserialized.getPayload(), original.getPayload());
}

// Test 8: MessageBuilder
TEST_F(ZMQMPCCommunicationTest, MessageBuilder) {
    MessageBuilder builder;
    
    uint32_t value1 = 12345;
    uint64_t value2 = 1234567890123456789ULL;
    std::string str = "Hello, MPC!";
    
    auto msg = builder.setType(Message::Type::CUSTOM)
                     .addUint32(value1)
                     .addUint64(value2)
                     .addString(str)
                     .build();
    
    EXPECT_EQ(msg.getType(), Message::Type::CUSTOM);
    
    // Verify the payload contains the expected data
    const auto& payload = msg.getPayload();
    size_t offset = 0;
    
    // Check uint32
    uint32_t readValue1 = payload[offset] | (payload[offset+1] << 8) | 
                         (payload[offset+2] << 16) | (payload[offset+3] << 24);
    EXPECT_EQ(readValue1, value1);
    offset += 4;
    
    // Check uint64
    uint64_t readValue2 = 0;
    for (int i = 0; i < 8; ++i) {
        readValue2 |= (static_cast<uint64_t>(payload[offset + i]) << (i * 8));
    }
    EXPECT_EQ(readValue2, value2);
    offset += 8;
    
    // Check string length
    uint32_t strLen = payload[offset] | (payload[offset+1] << 8) | 
                     (payload[offset+2] << 16) | (payload[offset+3] << 24);
    EXPECT_EQ(strLen, str.length());
    offset += 4;
    
    // Check string content
    std::string readStr(payload.begin() + offset, payload.begin() + offset + strLen);
    EXPECT_EQ(readStr, str);
}

// Test 9: Asynchronous message handling
TEST_F(ZMQMPCCommunicationTest, AsynchronousMessageHandler) {
    initializeAllParties();
    
    std::atomic<bool> messageReceived{false};
    uint32_t receivedSenderId = 0;
    std::vector<uint8_t> receivedData;
    
    // Set up async handler for party 2
    communications_[1]->setMessageHandler([&](uint32_t senderId, const Message& msg) {
        receivedSenderId = senderId;
        receivedData = msg.getPayload();
        messageReceived = true;
    });
    
    // Party 1 sends to Party 2
    std::vector<uint8_t> testData = {11, 22, 33};
    Message msg(Message::Type::DATA, testData);
    EXPECT_TRUE(communications_[0]->send(2, msg));
    
    // Wait for async handler
    for (int i = 0; i < 50 && !messageReceived; ++i) {
        std::this_thread::sleep_for(20ms);
    }
    
    EXPECT_TRUE(messageReceived);
    EXPECT_EQ(receivedSenderId, 1u);
    EXPECT_EQ(receivedData, testData);
}

// Test 10: Error handling
TEST_F(ZMQMPCCommunicationTest, ErrorHandler) {
    auto comm = createPartyComm(1);
    
    std::string lastError;
    comm->setErrorHandler([&](const std::string& error) {
        lastError = error;
    });
    
    // Try to send without initialization
    Message msg(Message::Type::DATA, {1, 2, 3});
    comm->send(2, msg);
    
    EXPECT_FALSE(lastError.empty());
    EXPECT_TRUE(lastError.find("not initialized") != std::string::npos);
}

// Test 11: Receive timeout
TEST_F(ZMQMPCCommunicationTest, ReceiveTimeout) {
    initializeAllParties();
    
    uint32_t senderId;
    Message msg;
    
    // Try to receive with short timeout (no message sent)
    auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(communications_[0]->receive(senderId, msg, 200ms));
    auto duration = std::chrono::steady_clock::now() - start;
    
    // Should timeout after approximately 200ms
    EXPECT_GE(duration, 180ms);
    EXPECT_LE(duration, 250ms);
}

// Test 12: Statistics
TEST_F(ZMQMPCCommunicationTest, Statistics) {
    initializeAllParties();
    
    // Send some messages
    Message msg1(Message::Type::DATA, {1, 2, 3, 4, 5});
    Message msg2(Message::Type::SHARE, {10, 20, 30});
    
    communications_[0]->send(2, msg1);
    communications_[0]->send(3, msg2);
    
    std::this_thread::sleep_for(100ms);
    
    auto stats = communications_[0]->getStatistics();
    
    // Verify stats contains expected fields
    EXPECT_TRUE(stats.find("\"party_id\": 1") != std::string::npos);
    EXPECT_TRUE(stats.find("\"messages_sent\": 2") != std::string::npos);
    EXPECT_TRUE(stats.find("\"bytes_sent\":") != std::string::npos);
}

// Test 13: Large message handling
TEST_F(ZMQMPCCommunicationTest, LargeMessageHandling) {
    initializeAllParties();
    
    // Create a large message (1MB)
    std::vector<uint8_t> largeData(1024 * 1024);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (auto& byte : largeData) {
        byte = static_cast<uint8_t>(dis(gen));
    }
    
    Message largeMsg(Message::Type::DATA, largeData);
    
    // Send large message
    EXPECT_TRUE(communications_[0]->send(2, largeMsg));
    
    // Receive and verify
    uint32_t senderId;
    Message receivedMsg;
    EXPECT_TRUE(communications_[1]->receive(senderId, receivedMsg, 5000ms));
    
    EXPECT_EQ(senderId, 1u);
    EXPECT_EQ(receivedMsg.getPayload(), largeData);
}

// Test 14: Concurrent sends and receives
TEST_F(ZMQMPCCommunicationTest, ConcurrentCommunication) {
    initializeAllParties();
    
    const int messagesPerParty = 10;
    std::atomic<int> totalReceived{0};
    
    // Each party will send messages to all others
    std::vector<std::thread> senders;
    for (size_t i = 0; i < numParties_; ++i) {
        senders.emplace_back([&, i]() {
            for (int j = 0; j < messagesPerParty; ++j) {
                for (size_t k = 0; k < numParties_; ++k) {
                    if (k != i) {
                        std::vector<uint8_t> data = {
                            static_cast<uint8_t>(i), 
                            static_cast<uint8_t>(j), 
                            static_cast<uint8_t>(k)
                        };
                        Message msg(Message::Type::DATA, data);
                        communications_[i]->send(k + 1, msg);
                    }
                }
                std::this_thread::sleep_for(10ms);
            }
        });
    }
    
    // Each party will receive messages
    std::vector<std::thread> receivers;
    for (size_t i = 0; i < numParties_; ++i) {
        receivers.emplace_back([&, i]() {
            int expectedMessages = messagesPerParty * (numParties_ - 1);
            for (int j = 0; j < expectedMessages; ++j) {
                uint32_t senderId;
                Message msg;
                if (communications_[i]->receive(senderId, msg, 5000ms)) {
                    totalReceived++;
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : senders) {
        t.join();
    }
    for (auto& t : receivers) {
        t.join();
    }
    
    // Each party sends to (numParties - 1) others, messagesPerParty times
    int expectedTotal = messagesPerParty * numParties_ * (numParties_ - 1);
    EXPECT_EQ(totalReceived, expectedTotal);
}

// Test 15: Queue management
TEST_F(ZMQMPCCommunicationTest, QueueManagement) {
    initializeAllParties();
    
    auto zmqComm = dynamic_cast<ZMQMPCCommunication*>(communications_[1].get());
    ASSERT_NE(zmqComm, nullptr);
    
    // Send multiple messages without receiving
    for (int i = 0; i < 5; ++i) {
        Message msg(Message::Type::DATA, {static_cast<uint8_t>(i)});
        communications_[0]->send(2, msg);
    }
    
    std::this_thread::sleep_for(200ms);
    
    // Check queue size
    EXPECT_EQ(zmqComm->getQueuedMessageCount(), 5u);
    
    // Receive all messages
    for (int i = 0; i < 5; ++i) {
        uint32_t senderId;
        Message msg;
        EXPECT_TRUE(communications_[1]->receive(senderId, msg, 100ms));
        EXPECT_EQ(msg.getPayload()[0], i);
    }
    
    EXPECT_EQ(zmqComm->getQueuedMessageCount(), 0u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
