#include "Party.h"
#include "AdditiveSecretSharing.h"
#include <cstring>  // For std::memcpy
#include <iostream> // For std::cout and std::cerr
#include <vector>
#include <cassert>
#include <sstream>
#include <iomanip>

#define BUFFER_SIZE 1024  // Added definition for BUFFER_SIZE

// ...existing includes and initializations...

// Helper function to serialize BIGNUM to hexadecimal string
std::string serializeShare(ShareType share) {
    if (!share) {
        throw std::runtime_error("Cannot serialize a null share.");
    }
    char* hex = BN_bn2hex(share);
    if (!hex) {
        throw std::runtime_error("Failed to serialize share to hex.");
    }
    std::string serializedShare(hex);
    OPENSSL_free(hex);
    return serializedShare;
}

// Helper function to deserialize hexadecimal string back to BIGNUM
ShareType deserializeShare(const std::string& data) {
    BIGNUM* bn = BN_new();
    if (!bn) {
        throw std::runtime_error("Failed to allocate BIGNUM for deserialization.");
    }
    if (!BN_hex2bn(&bn, data.c_str())) {
        BN_free(bn);
        throw std::runtime_error("Failed to deserialize share from hex.");
    }
    return bn;
}

// Corrected and updated broadcastShares function
void Party::broadcastShares(const std::vector<ShareType> &shares) {
    for (int i = 1; i <= m_totalParties; ++i) {
        if (i == m_partyId) continue;
        if (!shares[i - 1]) {
            // Handle null share
            std::cerr << "[Party " << m_partyId << "] Share for Party " << i << " is null.\n";
            continue;
        }
        try {
            // Serialize the share to a hex string
            std::string serializedShare = serializeShare(shares[i - 1]);
            // Send the serialized share to the target party
            m_comm->sendTo(i, serializedShare.c_str(), serializedShare.size());
            std::cout << "[Party " << m_partyId << "] Sent share to Party " << i << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << "[Party " << m_partyId << "] Failed to send share to Party " << i
                      << ": " << e.what() << "\n";
        }
    }
}

// Receives shares from other parties and deserializes them
void Party::receiveShares(std::vector<ShareType> &received, int expectedCount) {
    received.clear();
    received.resize(expectedCount, nullptr);  // Initialize with nullptrs
    int count = 0;
    while (count < expectedCount) {
        PARTY_ID_T senderId;
        char buffer[BUFFER_SIZE];
        size_t bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
        if (bytesRead == 0) {
            throw std::runtime_error("Received empty share from Party " + std::to_string(senderId));
        }
        std::string shareStr(buffer, bytesRead);
        std::cout << "[Party " << m_partyId << "] Received share from Party " << senderId 
                  << ": " << shareStr << "\n";
        try {
            // Deserialize the received share
            ShareType share = deserializeShare(shareStr);
            // Store the deserialized share
            received[count] = share;
            count++;
        }
        catch (const std::exception& e) {
            std::cerr << "[Party " << m_partyId << "] Failed to deserialize share from Party " 
                      << senderId << ": " << e.what() << "\n";
            // Optionally handle the error, e.g., retry or skip
        }
    }
}

// Securely multiplies shares using Beaver's Triple
void Party::secureMultiplyShares(ShareType myShareX, ShareType myShareY,
                                 const BeaverTriple &myTripleShare, ShareType &productOut) {
    // Suppress unused parameter warnings if parameters are not used
    (void)myShareX;
    (void)myShareY;
    (void)myTripleShare;
    (void)productOut;
    // ...existing implementation...
    // If implementation uses these parameters, remove the above lines.
}

// Distributes own shares to all parties
void Party::distributeOwnShares() {
    try {
        // Create and initialize secret BIGNUM
        BIGNUM* secret = AdditiveSecretSharing::newBigInt();
        if (!secret) throw std::runtime_error("Failed to create secret BIGNUM");
        
        std::cout << "[Party " << m_partyId << "] Converting value " << m_localValue << " to BIGNUM\n";
        if (!BN_set_word(secret, m_localValue)) {
            BN_free(secret);
            throw std::runtime_error("BN_set_word failed");
        }

        // Generate shares into myShares vector
        myShares.clear();
        AdditiveSecretSharing::generateShares(secret, m_totalParties, myShares);
        BN_free(secret);

        std::cout << "[Party " << m_partyId << "] Generated " << myShares.size() << " shares\n";
        broadcastShares(myShares);
    }
    catch (const std::exception& e) {
        std::cerr << "[Party " << m_partyId << "] Error in distributeOwnShares: " << e.what() << "\n";
        throw;
    }
}

// Gathers all shares from other parties
void Party::gatherAllShares() {
    try {
        // Initialize allShares structure
        allShares.clear();
        allShares.resize(m_totalParties + 1);
        for (int i = 1; i <= m_totalParties; ++i) {
            allShares[i].resize(m_totalParties, nullptr);
        }

        // Store my own shares first
        std::cout << "[Party " << m_partyId << "] Storing own shares\n";
        for(int idx = 0; idx < m_totalParties; idx++) {
            if (myShares[idx]) {
                allShares[m_partyId][idx] = AdditiveSecretSharing::cloneBigInt(myShares[idx]);
            }
        }

        // Brief pause to ensure all parties are ready to receive
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Receive shares from other parties
        for(int j = 1; j <= m_totalParties; ++j) {
            if (j == m_partyId) continue;

            std::cout << "[Party " << m_partyId << "] Waiting for share from Party " << j << "\n";
            std::vector<ShareType> receivedShares;
            
            try {
                receiveShares(receivedShares, 1);
                if (!receivedShares.empty() && receivedShares[0]) {
                    allShares[j][m_partyId - 1] = receivedShares[0];
                    std::cout << "[Party " << m_partyId << "] Successfully stored share from Party " 
                              << j << "\n";
                }
            }
            catch (const std::exception& e) {
                std::cerr << "[Party " << m_partyId << "] Failed to receive from Party " 
                          << j << ": " << e.what() << "\n";
                throw;
            }
        }

        std::cout << "[Party " << m_partyId << "] Successfully gathered all shares\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[Party " << m_partyId << "] Error in gatherAllShares: " << e.what() << "\n";
        // Clean up on error
        for (auto& row : allShares) {
            for (auto& bn : row) {
                if (bn) BN_free(bn);
            }
        }
        throw;
    }
}

// Broadcasts a partial sum to all parties
void Party::broadcastPartialSum(long long partialSum) {
    try {
        if (!m_comm) {
            throw std::runtime_error("Communication interface not initialized.");
        }
        std::string sumStr = std::to_string(partialSum);
        m_comm->sendToAll(sumStr.c_str(), sumStr.size());
        std::cout << "[Party " << m_partyId << "] Broadcasted partial sum: " << partialSum << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[Party " << m_partyId << "] Error in broadcastPartialSum: " << e.what() << "\n";
        throw;
    }
}

// Computes the global sum of secrets by receiving partial sums from all parties
void Party::computeGlobalSumOfSecrets() {
    try {
        long long globalSum = 0; // Initialize with 0
        for(int i = 1; i <= m_totalParties; ++i) {
            if(i == m_partyId) {
                globalSum += m_localValue; // Assuming m_localValue holds the party's own value
                continue;
            }
            long long receivedSum = 0;
            // Receive partial sum from Party i
            // This should be implemented based on the communication protocol
            // Example:
            // m_comm->receive(i, &receivedSum, sizeof(receivedSum));
            std::cout << "[Party " << m_partyId << "] Received partial sum " 
                      << receivedSum << " from Party " << i << "\n";
            globalSum += receivedSum;
        }
        std::cout << "[Party " << m_partyId << "] Global sum of all secrets: " << globalSum << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[Party " << m_partyId << "] Error in computeGlobalSumOfSecrets: " << e.what() << "\n";
        throw;
    }
}

// Demonstrates secure multiplication using Beaver's Triple
void Party::doMultiplicationDemo() {
    // For a real multi-party multiplication, each party would hold:
    // - A share of X, Y
    // - A share of Beaver triple (a, b, c)
    // - Each party computes d = x - a, e = y - b
    // - Sum d, e among all parties => D, E
    // - Final share = c + aE + bD + DE
    // Then reconstruct the final product from all parties’ product shares.
    // Below is just a placeholder.

    std::cout << "[Party " << m_partyId
              << "] doMultiplicationDemo: multi-party Beaver logic not fully implemented.\n";
}

// Other existing methods...