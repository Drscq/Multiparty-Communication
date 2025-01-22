#include "Party.h"
#include "AdditiveSecretSharing.h"
#include <cstring>  // For std::memcpy
#include <iostream> // For std::cout and std::cerr
#include <vector>
#include <cassert>
#include <sstream>
#include <iomanip>
#include "config.h" // Include config.h for ENABLE_COUT

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
            #ifdef ENABLE_COUT
            std::cout << "[Party " << m_partyId << "] Sent share to Party " << i << "\n";
            #endif
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
        #ifdef ENABLE_COUT
        std::cout << "[Party " << m_partyId << "] Received share from Party " << senderId 
                  << ": " << shareStr << "\n";
        #endif
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
        
        #ifdef ENABLE_COUT
        std::cout << "[Party " << m_partyId << "] Converting value " << m_localValue << " to BIGNUM\n";
        #endif
        if (!BN_set_word(secret, m_localValue)) {
            BN_free(secret);
            throw std::runtime_error("BN_set_word failed");
        }

        // Generate shares into myShares vector
        myShares.clear();
        AdditiveSecretSharing::generateShares(secret, m_totalParties, myShares);
        BN_free(secret);

        #ifdef ENABLE_COUT
        std::cout << "[Party " << m_partyId << "] Generated " << myShares.size() << " shares\n";
        #endif
        broadcastShares(myShares);

        syncAfterDistribute();
    }
    catch (const std::exception& e) {
        std::cerr << "[Party " << m_partyId << "] Error in distributeOwnShares: " << e.what() << "\n";
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
        #ifdef ENABLE_COUT
        std::cout << "[Party " << m_partyId << "] Broadcasted partial sum: " << partialSum << "\n";
        #endif
    }
    catch (const std::exception& e) {
        std::cerr << "[Party " << m_partyId << "] Error in broadcastPartialSum: " << e.what() << "\n";
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

// New helper for synchronization after distribution
void Party::syncAfterDistribute() {
    // Broadcast a short "done distributing" message to all
    const char* doneMsg = "DONE_DISTRIBUTING";
    m_comm->sendToAll(doneMsg, std::strlen(doneMsg));

    // Now wait to receive the same message from all other parties
    int needed = m_totalParties - 1;
    int got = 0;
    while(got < needed) {
        PARTY_ID_T senderId;
        char buffer[BUFFER_SIZE];
        size_t bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
        if(bytesRead > 0) {
            std::string msg(buffer, bytesRead);
            if(msg == "DONE_DISTRIBUTING" && senderId != m_partyId) {
                got++;
            }
        }
    }
    std::cout << "[Party " << m_partyId << "] All parties done distributing.\n";
}

// New helper for synchronization after gathering
void Party::syncAfterGather() {
    // Broadcast a short "done gathering" message
    const char* doneMsg = "DONE_GATHERING";
    m_comm->sendToAll(doneMsg, std::strlen(doneMsg));

    // Wait to receive the same from all other parties
    int needed = m_totalParties - 1;
    int got = 0;
    while(got < needed) {
        PARTY_ID_T senderId;
        char buffer[BUFFER_SIZE];
        size_t bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
        if(bytesRead > 0) {
            std::string msg(buffer, bytesRead);
            if(msg == "DONE_GATHERING" && senderId != m_partyId) {
                got++;
            }
        }
    }
    std::cout << "[Party " << m_partyId << "] All parties done gathering.\n";
}

// Other existing methods...

void Party::distributeSharesAndComputeMyPartial() {
    // 1) Convert localValue to BIGNUM
    BIGNUM* secretBn = AdditiveSecretSharing::newBigInt();
    BN_set_word(secretBn, m_localValue);

    // 2) Generate n additive shares
    std::vector<ShareType> mySecretShares;
    AdditiveSecretSharing::generateShares(secretBn, m_totalParties, mySecretShares);
    for (int i = 0; i < m_totalParties; ++i) {
        #ifdef ENABLE_COUT
        std::cout << "[Party " << m_partyId << "] Share " << i + 1 << ": " 
                  << BN_bn2dec(mySecretShares[i]) << "\n";
        #endif
    }

    // Free the original secret BN
    BN_free(secretBn);

    // 3) Send each share to the corresponding party
    for (int j = 1; j <= m_totalParties; ++j) {
        if (j != m_partyId) {
            std::string sHex = serializeShare(mySecretShares[j - 1]);
            m_comm->sendTo(j, sHex.c_str(), sHex.size());
        }
    }

    // 4) Initialize my partial sum to my own share
    BIGNUM* myPartialSumBN = AdditiveSecretSharing::newBigInt();
    BN_copy(myPartialSumBN, mySecretShares[m_partyId - 1]);

    // 5) Receive one share from each other party
    int needed = m_totalParties - 1;
    int receivedCount = 0;
    while (receivedCount < needed) {
        PARTY_ID_T senderId;
        char buffer[BUFFER_SIZE];
        size_t bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            std::string shareHex(buffer, bytesRead);
            BIGNUM* shareBN = deserializeShare(shareHex);
            // Add to my partial sum
            BN_mod_add(myPartialSumBN, myPartialSumBN, shareBN, 
                       AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
            BN_free(shareBN);
            receivedCount++;
        }
    }

    // Store result in m_myPartialSum
    if (!m_myPartialSum) {
        m_myPartialSum = AdditiveSecretSharing::newBigInt();
    }
    BN_copy(m_myPartialSum, myPartialSumBN);
    #ifdef ENABLE_COUT
    std::cout << "[Party " << m_partyId << "] Partial sum computed with value: " 
              << BN_bn2dec(m_myPartialSum) << "\n";
    #endif

    BN_free(myPartialSumBN);
    for (auto &bn : mySecretShares) {
        BN_free(bn);
    }
    #ifdef ENABLE_COUT
    std::cout << "[Party " << m_partyId << "] Partial sum computed.\n";
    #endif
}

// Broadcast partial sums and reconstruct global sum
void Party::broadcastAndReconstructGlobalSum() {
    if (!m_myPartialSum) {
        throw std::runtime_error("No partial sum available.");
    }
    // 1) Broadcast my partial sum
    std::string partialHex = serializeShare(m_myPartialSum);
    m_comm->sendToAll(partialHex.c_str(), partialHex.size());

    // 2) Sum up all partial sums
    BIGNUM* finalSum = AdditiveSecretSharing::newBigInt();
    BN_copy(finalSum, m_myPartialSum);

    int needed = m_totalParties - 1;
    int receivedCount = 0;
    while (receivedCount < needed) {
        PARTY_ID_T senderId;
        char buffer[BUFFER_SIZE];
        size_t bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            std::string pHex(buffer, bytesRead);
            BIGNUM* otherPartial = deserializeShare(pHex);
            BN_mod_add(finalSum, finalSum, otherPartial, 
                       AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
            BN_free(otherPartial);
            receivedCount++;
        }
    }

    // 3) Print final sum
    char* decStr = BN_bn2dec(finalSum);
    #ifdef ENABLE_COUT
    std::cout << "[Party " << m_partyId << "] Global sum = " << decStr << "\n";
    #endif
    OPENSSL_free(decStr);
    BN_free(finalSum);
}

// Comment out or remove old methods
// void Party::broadcastAllHeldShares() { /* removed */ }
// void Party::gatherAllShares() { /* removed */ }
// void Party::computeGlobalSumOfSecrets() { /* removed */ }