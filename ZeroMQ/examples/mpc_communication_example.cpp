#include "../src/ZMQMPCCommunication.h"
#include "../src/Message.cpp"
#include "../src/ZMQMPCCommunication.cpp"
#include <iostream>
#include <thread>
#include <vector>

using namespace mpc;

// Example MPC protocol using the communication layer
class MPCParty {
public:
    MPCParty(uint32_t id, std::unique_ptr<IMPCCommunication> comm) 
        : partyId_(id), comm_(std::move(comm)) {}
    
    void initialize() {
        std::cout << "[Party " << partyId_ << "] Initializing...\n";
        
        // Set up message handler
        comm_->setMessageHandler([this](uint32_t senderId, const Message& msg) {
            handleMessage(senderId, msg);
        });
        
        // Set up error handler
        comm_->setErrorHandler([this](const std::string& error) {
            std::cerr << "[Party " << partyId_ << "] Error: " << error << "\n";
        });
        
        // Initialize communication
        comm_->initialize();
        
        // Enable logging for demo
        if (auto zmqComm = dynamic_cast<ZMQMPCCommunication*>(comm_.get())) {
            zmqComm->enableLogging(true);
        }
    }
    
    // Example: Share distribution phase
    void distributeShares(uint32_t secret) {
        std::cout << "[Party " << partyId_ << "] Distributing shares of secret: " << secret << "\n";
        
        // Simple additive sharing: split secret into n-1 random shares
        std::vector<uint32_t> shares;
        uint32_t sum = 0;
        
        for (size_t i = 1; i < comm_->getTotalParties(); ++i) {
            uint32_t share = std::rand() % 1000;
            shares.push_back(share);
            sum += share;
        }
        
        // Last share ensures sum equals secret
        shares.push_back(secret - sum);
        
        // Send shares to other parties
        MessageBuilder builder;
        size_t shareIdx = 0;
        
        for (uint32_t pid = 1; pid <= comm_->getTotalParties(); ++pid) {
            if (pid != partyId_) {
                auto msg = builder.setType(Message::Type::SHARE)
                                 .addUint32(shares[shareIdx++])
                                 .build();
                
                comm_->send(pid, msg);
            }
        }
        
        // Keep own share
        myShare_ = shares.back();
        std::cout << "[Party " << partyId_ << "] My share: " << myShare_ << "\n";
    }
    
    // Example: Collect shares and reconstruct
    void collectSharesAndReconstruct() {
        std::cout << "[Party " << partyId_ << "] Collecting shares...\n";
        
        uint32_t totalShares = myShare_;
        uint32_t sharesReceived = 0;
        uint32_t expectedShares = comm_->getTotalParties() - 1;
        
        while (sharesReceived < expectedShares) {
            uint32_t senderId;
            Message msg;
            
            if (comm_->receive(senderId, msg, std::chrono::seconds(5))) {
                if (msg.getType() == Message::Type::SHARE) {
                    uint32_t share = extractUint32(msg.getPayload(), 0);
                    totalShares += share;
                    sharesReceived++;
                    
                    std::cout << "[Party " << partyId_ 
                             << "] Received share from party " << senderId 
                             << ": " << share << "\n";
                }
            }
        }
        
        std::cout << "[Party " << partyId_ 
                 << "] Reconstructed secret: " << totalShares << "\n";
    }
    
    // Demonstrate broadcast functionality
    void broadcastAnnouncement(const std::string& message) {
        MessageBuilder builder;
        auto msg = builder.setType(Message::Type::CONTROL)
                         .addString(message)
                         .build();
        
        std::cout << "[Party " << partyId_ << "] Broadcasting: " << message << "\n";
        comm_->broadcast(msg);
    }
    
    void shutdown() {
        comm_->shutdown();
        std::cout << "[Party " << partyId_ << "] Shutdown complete\n";
    }
    
    void printStatistics() {
        std::cout << "[Party " << partyId_ << "] Statistics: " 
                 << comm_->getStatistics() << "\n";
    }
    
private:
    void handleMessage(uint32_t senderId, const Message& msg) {
        switch (msg.getType()) {
            case Message::Type::CONTROL: {
                auto message = extractString(msg.getPayload(), 0);
                std::cout << "[Party " << partyId_ 
                         << "] Received announcement from party " << senderId 
                         << ": " << message << "\n";
                break;
            }
            default:
                // Handle other message types
                break;
        }
    }
    
    uint32_t extractUint32(const std::vector<uint8_t>& data, size_t offset) {
        if (offset + 4 > data.size()) return 0;
        return data[offset] | (data[offset+1] << 8) | 
               (data[offset+2] << 16) | (data[offset+3] << 24);
    }
    
    std::string extractString(const std::vector<uint8_t>& data, size_t offset) {
        uint32_t length = extractUint32(data, offset);
        offset += 4;
        
        if (offset + length > data.size()) return "";
        return std::string(data.begin() + offset, data.begin() + offset + length);
    }
    
private:
    uint32_t partyId_;
    std::unique_ptr<IMPCCommunication> comm_;
    uint32_t myShare_ = 0;
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <party_id> <total_parties> <secret_value>\n";
        return 1;
    }
    
    uint32_t partyId = std::stoul(argv[1]);
    uint32_t totalParties = std::stoul(argv[2]);
    uint32_t secretValue = std::stoul(argv[3]);
    
    // Setup party endpoints
    std::map<uint32_t, std::string> endpoints;
    int basePort = 12000;
    
    for (uint32_t i = 1; i <= totalParties; ++i) {
        endpoints[i] = "tcp://127.0.0.1:" + std::to_string(basePort + i);
    }
    
    try {
        // Create MPC party
        auto comm = createZMQMPCCommunication(partyId, endpoints);
        MPCParty party(partyId, std::move(comm));
        
        // Initialize
        party.initialize();
        
        // Wait for all parties to connect
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Phase 1: Announce participation
        party.broadcastAnnouncement("Ready for MPC protocol");
        
        // Wait a bit
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Phase 2: Share distribution
        if (partyId == 1) {
            // Party 1 distributes shares of the secret
            party.distributeShares(secretValue);
        }
        
        // Phase 3: Reconstruction (all parties)
        party.collectSharesAndReconstruct();
        
        // Print statistics
        party.printStatistics();
        
        // Cleanup
        party.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
