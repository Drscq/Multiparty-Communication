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

        syncAfterDistribute();
    }
    catch (const std::exception& e) {
        std::cerr << "[Party " << m_partyId << "] Error in distributeOwnShares: " << e.what() << "\n";
        throw;
    }
}

// New helper: broadcast every share we currently hold for every secret j.
void Party::broadcastAllHeldShares() {
    // For each secret j in [1..m_totalParties], each share from col k in [0..m_totalParties-1]
    for(int j = 1; j <= m_totalParties; ++j) {
        for(int k = 0; k < m_totalParties; ++k) {
            ShareType share = allShares[j][k];
            if(!share) continue; // Skip missing or null shares

            try {
                // Serialize the share into hex
                std::string s = serializeShare(share);
                // Each broadcast will include which secret j and which “column” k
                // so the receiver knows where to store it.
                // We’ll format it like: "j|k|<hex>"
                std::string msg = std::to_string(j) + "|" + std::to_string(k) + "|" + s;
                m_comm->sendToAll(msg.c_str(), msg.size());
            }
            catch(const std::exception &e) {
                std::cerr << "[Party " << m_partyId << "] Error broadcasting share of secret " 
                          << j << " col " << k << ": " << e.what() << "\n";
            }
        }
    }
    std::cout << "[Party " << m_partyId << "] broadcastAllHeldShares() completed\n";
}

// Modified gatherAllShares(): call broadcastAllHeldShares() after storing our own shares,
// then receive re-broadcasts from all parties. This ensures each party gets all s_{j,k}.
void Party::gatherAllShares() {
    try {
        // 1) Prepare allShares
        allShares.clear();
        allShares.resize(m_totalParties + 1, std::vector<ShareType>(m_totalParties, nullptr));

        // 2) Store our local secret's shares in row [m_partyId]
        for(int idx = 0; idx < m_totalParties; idx++) {
            if (myShares[idx]) {
                allShares[m_partyId][idx] = AdditiveSecretSharing::cloneBigInt(myShares[idx]);
            }
        }

        // 3) Brief pause so all parties finish distributing their own secrets
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 4) Each party now broadcasts everything it holds so far
        broadcastAllHeldShares();

        // 5) Receive enough shares to fill allShares[][] for secrets j=1..N, k=0..N-1.
        // We need (m_totalParties - 1)*m_totalParties more pieces at most
        int needed = m_totalParties * m_totalParties; // rough upper bound
        int receivedCount = 0;
        int idleCount = 0; // Track consecutive idle loops
        const int IDLE_LIMIT = 20; // E.g., 20 tries, tweak as needed

        while(receivedCount < needed && idleCount < IDLE_LIMIT) {
            PARTY_ID_T senderId;
            char buffer[BUFFER_SIZE];
            size_t bytesRead = 0;

            // Try a non-blocking receive or short blocking with a try/catch
            try {
                bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
            } catch(...) {
                // No message this round
                bytesRead = 0;
            }

            if (bytesRead == 0) {
                // No new data, increment idleCount
                idleCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // small wait
                continue;
            } else {
                idleCount = 0; // reset idle
            }

            std::string fullMsg(buffer, bytesRead);
            // Expect format: "j|k|<hex>"
            size_t pos1 = fullMsg.find('|');
            size_t pos2 = fullMsg.find('|', pos1 + 1);
            if(pos1 == std::string::npos || pos2 == std::string::npos) {
                continue;
            }

            int secretOwner = std::stoi(fullMsg.substr(0, pos1));
            int colIndex = std::stoi(fullMsg.substr(pos1 + 1, pos2 - (pos1 + 1)));
            std::string hexVal = fullMsg.substr(pos2 + 1);

            try {
                ShareType incoming = deserializeShare(hexVal);
                if(allShares[secretOwner][colIndex] == nullptr) {
                    allShares[secretOwner][colIndex] = incoming;
                    receivedCount++;
                } else {
                    BN_free(incoming); // discard duplicates
                }
            }
            catch(const std::exception &e) {
                std::cerr << "[Party " << m_partyId << "] Failed to deserialize broadcast: " 
                          << e.what() << "\n";
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

    syncAfterGather();
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

// Reconstructs all secrets from the shares and computes the global sum
void Party::computeGlobalSumOfSecrets() {
    try {
        BIGNUM* globalSum = AdditiveSecretSharing::newBigInt(); // Initialize to 0
        BN_zero(globalSum);

        // Iterate over each party to reconstruct their secret and add to global sum
        for(int j = 1; j <= m_totalParties; ++j) {
            if(j == m_partyId) {
                // Add own local value directly
                BIGNUM* ownValueBN = AdditiveSecretSharing::newBigInt();
                if(!BN_set_word(ownValueBN, m_localValue)) {
                    BN_free(ownValueBN);
                    throw std::runtime_error("BN_set_word failed for own value");
                }
                BN_mod_add(globalSum, globalSum, ownValueBN, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
                BN_free(ownValueBN);
                continue;
            }

            // Check if all shares for Party j's secret are available
            bool allSharesAvailable = true;
            for(int k = 1; k <= m_totalParties; ++k) {
                // Each secret j should have shares s_{j1}, s_{j2}, ..., s_{jn}
                // Here, s_{jk} is stored in allShares[j][k-1]
                if(allShares[j][k - 1] == nullptr) {
                    std::cerr << "[Party " << m_partyId << "] Missing share from Party " 
                              << k << " for secret of Party " << j << "\n";
                    allSharesAvailable = false;
                    break;
                }
            }

            if(!allSharesAvailable) {
                throw std::runtime_error("Incomplete shares for Party " + std::to_string(j));
            }

            // Reconstruct secret from shares
            BIGNUM* secret = AdditiveSecretSharing::newBigInt();
            AdditiveSecretSharing::reconstructSecret(allShares[j], secret);

            // Add the reconstructed secret to the global sum
            BN_mod_add(globalSum, globalSum, secret, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());

            BN_free(secret);
        }

        // Convert globalSum to a human-readable format
        char* sumStr = BN_bn2dec(globalSum);
        if(!sumStr) {
            BN_free(globalSum);
            throw std::runtime_error("Failed to convert global sum to decimal string");
        }

        std::cout << "[Party " << m_partyId << "] Global sum of all secrets: " << sumStr << "\n";
        OPENSSL_free(sumStr);
        BN_free(globalSum);
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

// New helper for synchronization after distribution
void Party::syncAfterDistribute() {
    // Broadcast a short “done distributing” message to all
    const char* doneMsg = "DONE_DISTRIBUTING";
    m_comm->sendToAll(doneMsg, std::strlen(doneMsg));

    // Now wait to receive the same message from all other parties
    int needed = m_totalParties - 1;
    int got = 0;
    while(got < needed) {
        PARTY_ID_T senderId;
        char buffer[BUFFER_SIZE];
        size_t bytesRead = 0;
        bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
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
    // Broadcast a short “done gathering” message
    const char* doneMsg = "DONE_GATHERING";
    m_comm->sendToAll(doneMsg, std::strlen(doneMsg));

    // Wait to receive the same from all other parties
    int needed = m_totalParties - 1;
    int got = 0;
    while(got < needed) {
        PARTY_ID_T senderId;
        char buffer[BUFFER_SIZE];
        size_t bytesRead = 0;
        bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
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