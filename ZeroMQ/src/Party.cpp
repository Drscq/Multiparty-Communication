#include "Party.h"
#include "AdditiveSecretSharing.h"
#include <cstring>  // For std::memcpy
#include <iostream> // For std::cout and std::cerr
#include <vector>
#include <cassert>
#include <sstream>
#include <iomanip>
#include "config.h" // Include config.h for ENABLE_COUT
#include <zmq.hpp>

#define BUFFER_SIZE (1024)  // 1 KB buffer

Party::~Party() {
    {
        if (m_myPartialSum) BN_free(m_myPartialSum);
        for (auto &secret : m_secrets) {
            if (secret) BN_free(secret);
        }
        #if defined(ENABLE_MALICIOUS_SECURITY)
        if (m_global_mac_key) BN_free(m_global_mac_key);
        for (auto &shares : m_macShares) {
            for (auto &share : shares) {
                if (share) BN_free(share);
            }
        }
        for (auto &shares : m_addition_partial_sum) {
            for (auto &share : shares) {
                if (share) BN_free(share);
            }
        }
        if (m_secret_sum) BN_free(m_secret_sum);
        if (m_secret_sum_mac) BN_free(m_secret_sum_mac);
        if (m_epsilon) BN_free(m_epsilon);
        if (m_rho) BN_free(m_rho);
        // if (m_z_i_mac) BN_free(m_z_i_mac);
        if (m_global_key_share) BN_free(m_global_key_share);
        if (m_sigma) BN_free(m_sigma);
        #endif // ENABLE_MALICIOUS_SECURITY
    }
}

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
    ShareType bn = BN_new();
    if (!bn) {
        throw std::runtime_error("Failed to allocate BIGNUM for deserialization.");
    }
    if (!BN_hex2bn(&bn, data.c_str())) {
        BN_free(bn);
        throw std::runtime_error("Failed to deserialize share from hex.");
    }
    return bn;
}

void Party::init() {
    // Optionally do extra setup here
    #ifdef ENABLE_COUT
    std::cout << "[Party " << m_partyId << "] init called.\n";
    #endif
    if (m_hasSecret) {
        // Generate the global key to be used for MAC values
        #if defined(ENABLE_MALICIOUS_SECURITY)
        BN_rand_range(m_global_mac_key, AdditiveSecretSharing::getPrime());
        #if defined(ENABLE_UNIT_TESTS)
        BN_set_word(m_global_mac_key, 2);
        std::cout << "[Party " << m_partyId << "] Global MAC key: " << BN_bn2dec(m_global_mac_key) << "\n";
        #endif
        #endif

        this->broadcastAllData(&CMD_SEND_SHARES, sizeof(CMD_T));
        // Prepare the ShareType secrets for this party by initialing two secrets into the ShareType array
        // std::vector<ShareType> secrets;
        for (int i = 0; i < NUM_SECRETS; ++i) {
            ShareType secret_share_type = AdditiveSecretSharing::newBigInt();
            BN_set_word(secret_share_type, m_localValue + i);
            // secrets.push_back(secret_share_type);
            // secrets.emplace_back(secret_share_type);
            m_secrets[i] = secret_share_type;
            #if defined(ENABLE_COUT)
            std::cout << "[Party " << m_partyId << "] Secret share type value: " << BN_bn2dec(secret_share_type) << "\n";
            #endif
        }
        // Generate shares for the secrets
        std::unordered_map<ShareType, std::vector<ShareType>> shares;
        this->generateMyShares(m_secrets, shares);
        #if defined(ENABLE_UNIT_TESTS)
        // Cout the Shares 
        for (auto &secret : shares) {
            std::cout << "[Party " << m_partyId << "] Shares for secret " << BN_bn2dec(secret.first) << ":\n";
            for (auto &share : secret.second) {
                std::cout << "  " << BN_bn2dec(share) << "\n";
            }
        }
        #endif
        #if defined(ENABLE_MALICIOUS_SECURITY)
        // Generate the MAC secret with its corresponding shares
        for (int i = 0; i < NUM_SECRETS; ++i) {
            std::cout << "[Party " << m_partyId << "] Generating MAC shares for secret " << i << "\n";
            AdditiveSecretSharing::generateMacShares(m_secrets[i], m_global_mac_key, m_totalParties, m_macShares[i]);
        }
            #if defined(ENABLE_UNIT_TESTS)
            // Reconstrcut the MAC shares and print the results
            for (int i = 0; i < NUM_SECRETS; ++i) {
                ShareType macShare = AdditiveSecretSharing::newBigInt();
                AdditiveSecretSharing::reconstructSecret(m_macShares[i], macShare);
                std::cout << "[Party " << m_partyId << "] Reconstructed MAC share for secret " << i << ": " << BN_bn2dec(macShare) << "\n";
                BN_free(macShare);
            }
            #endif
        #endif 

        // Broadcast the shares to all parties
        for (PARTY_ID_T j = 1; j <= m_totalParties; ++j) {
            // Serialize each share separately and send as a structured message
            std::ostringstream shareStream;
            for (int i = 0; i < NUM_SECRETS; ++i) {
                shareStream << serializeShare(shares[m_secrets[i]][j - 1]);
                if (i < NUM_SECRETS - 1) {
                    shareStream << "|"; // Delimiter between shares
                }
            }
            #if defined(ENABLE_MALICIOUS_SECURITY)
            for (int i = 0; i < NUM_SECRETS; ++i) {
                shareStream << "|"; // Delimiter between secrets and MAC shares
                shareStream << serializeShare(m_macShares[i][j - 1]);
                if (i < NUM_SECRETS - 1) {
                    shareStream << "|"; // Delimiter between MAC shares
                }
            }
            #endif
            std::string shareStr = shareStream.str();

            // Add error handling for send operation
            try {
                m_comm->sendTo(j, shareStr.c_str(), shareStr.size());
                #ifdef ENABLE_COUT
                std::cout << "[Party " << m_partyId << "] Sent shares to Party " << j << "\n";
                #endif
            } catch (const std::exception& e) {
                std::cerr << "[Party " << m_partyId << "] Failed to send shares to Party " << j 
                          << ": " << e.what() << "\n";
            }
        }
        // Sync after distributing shares
        for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
            m_comm->dealerReceive(i, &m_cmd, sizeof(CMD_T));
            if (m_cmd == CMD_SUCCESS) {
                std::cout << "[Party " << m_partyId << "] Received success from Party " << i << "\n";
            }
        }
        this->broadcastAllData(&CMD_ADDITION, sizeof(CMD_T));
        #if defined(ENABLE_MALICIOUS_SECURITY)
        std::vector<ShareType> receivedSecretPartialSumShares(m_totalParties);
        std::vector<ShareType> receivedMacPartialSumShares(m_totalParties);
        for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
            char buffer[BUFFER_SIZE];
            size_t bytesRead = m_comm->dealerReceive(i, buffer, sizeof(buffer));
            if (bytesRead == 0) {
                throw std::runtime_error("Received empty message from Party " + std::to_string(i));
            }
            // Split the received message into secret and MAC shares
            std::string partialSumStr(buffer, bytesRead);
            std::vector<std::string> shareParts;
            std::stringstream ss(partialSumStr);
            std::string item;
            while (std::getline(ss, item, '|')) {
                if (!item.empty()) {
                    shareParts.push_back(item);
                }
            }
            if (shareParts.size() != 2) {
                throw std::runtime_error("Received invalid number of shares from Party " + std::to_string(i));
            }
            // Deserialize the secret and MAC shares
            for (int ii = 0; ii < NUM_TWO; ++ii) {
                m_addition_partial_sum[ii][i - 1] = deserializeShare(shareParts[ii]);
            }
        }
        #if defined(ENABLE_UNIT_TESTS)
        // check the received partial sums
        for (int i = 0; i < NUM_TWO; ++i) {
            if (i == 0) {
                std::cout << "Partial secrets sum " << std::endl;
            } else {
                std::cout << "Partial MAC sum " << std::endl;
            }
            for (PARTY_ID_T j = 1; j <= m_totalParties; ++j) {
                std::cout << "[Party " << m_partyId << "] Received partial sum " << i << " from Party " << j << ": " << BN_bn2dec(m_addition_partial_sum[i][j - 1]) << "\n";
            }
        }
        #endif
        // Reconstruct the global sum for the secrets and MAC shares
        AdditiveSecretSharing::reconstructSecret(m_addition_partial_sum[0], m_secret_sum);
        AdditiveSecretSharing::reconstructSecret(m_addition_partial_sum[1], m_secret_sum_mac);
        // Print the global sum
        #if defined(ENABLE_FINAL_RESULT)
        std::cout << "[Party " << m_partyId << "] Global secret sum: " << BN_bn2dec(m_secret_sum) << "\n";
        std::cout << "[Party " << m_partyId << "] Global MAC sum: " << BN_bn2dec(m_secret_sum_mac) << "\n";
        #endif
        // use the assert to check the equality of the m_global_mac_key * m_secret_sum and m_secret_sum_mac
        ShareType macAdditionProduct = AdditiveSecretSharing::newBigInt();
        BN_mod_mul(macAdditionProduct, m_global_mac_key, m_secret_sum, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
        assert(BN_cmp(macAdditionProduct, m_secret_sum_mac) == 0 && "The MAC product is not equal to the MAC sum");
        BN_free(macAdditionProduct);
        #else
            std::vector<ShareType> receivedParitialSums(m_totalParties);
            for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
                char buffer[BUFFER_SIZE];
                size_t bytesRead = m_comm->dealerReceive(i, buffer, sizeof(buffer));
                if (bytesRead > 0) {
                    std::string partialSumStr(buffer, bytesRead);
                    ShareType partialSum = deserializeShare(partialSumStr);
                    receivedParitialSums[i - 1] = partialSum;
                }
            }
            // check the received partial sums
            #if defined(ENABLE_UNIT_TESTS)
            for (auto &partialSum : receivedParitialSums) {
                std::cout << "[Party " << m_partyId << "] Received partial sum from Party " << BN_bn2dec(partialSum) << "\n";
            }
            #endif
            // Reconstruct the global sum
            ShareType globalSum = AdditiveSecretSharing::newBigInt();
            AdditiveSecretSharing::reconstructSecret(receivedParitialSums, globalSum);
            // Print the global sum
            #if defined(ENABLE_FINAL_RESULT)
            std::cout << "[Party " << m_partyId << "] Global sum: " << BN_bn2dec(globalSum) << "\n";
            #endif
            // Free the global sum
            BN_free(globalSum);
        #endif
        std::this_thread::sleep_for(std::chrono::seconds(1));
        this->broadcastAllData(&CMD_MULTIPLICATION, sizeof(CMD_T));
        this->distributeBeaverTriple();
        // Sync after distributing shares
        for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
            m_comm->dealerReceive(i, &m_cmd, sizeof(CMD_T));
            if (m_cmd == CMD_SUCCESS) {
                std::cout << "[distributeBeaverTriple][Party " << m_partyId << "] Received success from Party " << i << "\n";
            }
        }
         // Sync after distributing shares
        for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
            m_comm->dealerReceive(i, &m_cmd, sizeof(CMD_T));
            if (m_cmd == CMD_SUCCESS) {
                std::cout << "[MultipliationDone][Party " << m_partyId << "] Received success from Party " << i << "\n";
            }
        }
        // make a pause to allow the dealer to send the triple shares
        // std::this_thread::sleep_for(std::chrono::seconds(2));
        this->broadcastAllData(&CMD_FETCH_MULT_SHARE, sizeof(CMD_T));
        // Sync after distributing shares
        for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
            m_comm->dealerReceive(i, &m_cmd, sizeof(CMD_T));
            if (m_cmd == CMD_SUCCESS) {
                std::cout << "[Party " << m_partyId << "] Received success from Party " << i << "\n";
            }
        }
        m_receivedMultiplicationShares.reserve(m_totalParties);
        m_receivedMultiplicationShares.resize(m_totalParties);
        for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
            #if defined(ENABLE_UNIT_TESTS)
            std::cout << "[Party " << m_partyId << "] Receiving multiplication shares from Party " << i << "\n";
            #endif
            char buffer[BUFFER_SIZE];
            size_t bytesRead = m_comm->dealerReceive(i, buffer, sizeof(buffer));
            if (bytesRead > 0) {
                std::string shareStr(buffer, bytesRead);
                ShareType share = deserializeShare(shareStr);
                m_receivedMultiplicationShares[i - 1] = share;
            }
        }
        // Check the values in the multiplication shares
        #if defined(ENABLE_UNIT_TESTS)
        for (auto &share : m_receivedMultiplicationShares) {
            std::cout << "[Party " << m_partyId << "] Received multiplication share: " << BN_bn2dec(share) << "\n";
        }
        #endif
        // Use the multiplication shares to compute the final product
        ShareType product = AdditiveSecretSharing::newBigInt();
        AdditiveSecretSharing::reconstructSecret(m_receivedMultiplicationShares, product);
        // Print the final product
        #if defined(ENABLE_FINAL_RESULT)
        std::cout << "[Party " << m_partyId << "] Final product: " << BN_bn2dec(product) << "\n";
        #endif
        #if defined(ENABLE_MALICIOUS_SECURITY)
        // Receive the MAC shares from all parties and Check the MAC product
        for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
            #if defined(ENABLE_UNIT_TESTS)
            std::cout << "[Party " << m_partyId << "] Receiving MAC shares from Party " << i << "\n";
            #endif
            char buffer[BUFFER_SIZE];
            size_t bytesRead = m_comm->dealerReceive(i, buffer, sizeof(buffer));
            if (bytesRead > 0) {
                std::string shareStr(buffer, bytesRead);
                ShareType share = deserializeShare(shareStr);
                m_receivedMultiplicationMacShares[i - 1] = share;
            }
        }
            #if defined(ENABLE_UNIT_TESTS)
            std::cout << "Received MAC shares" << std::endl;
            for (auto &share : m_receivedMultiplicationMacShares) {
                std::cout << "[Party " << m_partyId << "] Received MAC share: " << BN_bn2dec(share) << "\n";
            }
            #endif
        ShareType macProduct = AdditiveSecretSharing::newBigInt();
        AdditiveSecretSharing::reconstructSecret(m_receivedMultiplicationMacShares, macProduct);
        // Print the final product
            #if defined(ENABLE_FINAL_RESULT)
            std::cout << "[Party " << m_partyId << "] Final MAC product: " << BN_bn2dec(macProduct) << "\n";
            #endif
        // assert the equality of the product multiplied by the global MAC key and the MAC product
        ShareType macProductCheck = AdditiveSecretSharing::newBigInt();
        BN_mod_mul(macProductCheck, product, m_global_mac_key, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
        assert(BN_cmp(macProductCheck, macProduct) == 0 && "The MAC product is not equal to the MAC sum");
        BN_free(macProduct);
        BN_free(macProductCheck);
        // Receive the sigma shares from all parties
        for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
            #if defined(ENABLE_UNIT_TESTS)
            std::cout << "[Party " << m_partyId << "] Receiving sigma shares from Party " << i << "\n";
            #endif
            char buffer[BUFFER_SIZE];
            size_t bytesRead = m_comm->dealerReceive(i, buffer, sizeof(buffer));
            if (bytesRead > 0) {
                std::string shareStr(buffer, bytesRead);
                ShareType share = deserializeShare(shareStr);
                m_receivedMultiplicationSigmaShares[i - 1] = share;
            }
        }
            #if defined(ENABLE_UNIT_TESTS)
            std::cout << "Received sigma shares" << std::endl;
            for (auto &share : m_receivedMultiplicationSigmaShares) {
                std::cout << "[Party " << m_partyId << "] Received sigma share: " << BN_bn2dec(share) << "\n";
            }
            #endif
        // Reconstruct the sigma product and check it is equal to the zero or not
        ShareType sigmaProduct = AdditiveSecretSharing::newBigInt();
        AdditiveSecretSharing::reconstructSecret(m_receivedMultiplicationSigmaShares, sigmaProduct);
        // Print the final product
            #if defined(ENABLE_FINAL_RESULT)
            std::cout << "[Party " << m_partyId << "] Final sigma product: " << BN_bn2dec(sigmaProduct) << "\n";
            #endif
        // assert the equality of the sigma product and the zero
        assert(BN_is_zero(sigmaProduct) && "The sigma product is not equal to zero");
        #endif

        BN_free(product);

        this->broadcastAllData(&CMD_SHUTDOWN, sizeof(CMD_T));
    } else {
        this->runEventLoop();
    }

    // Now party init is simpler, no direct broadcasting or looping.
}

void Party::broadcastAllData(const void* data, LENGTH_T length) {
    for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
        m_comm->sendTo(i, data, length);
    }
}
void Party::receiveAllData(void* data, LENGTH_T length) {
    for (PARTY_ID_T i = 1; i <= m_totalParties; ++i) {
        m_comm->dealerReceive(i, data, length);
    }
}

// Corrected and updated broadcastShares function
void Party::broadcastShares(const std::vector<ShareType> &shares) {
    for (int i = 1; i <= m_totalParties; ++i) {
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
    received.reserve(expectedCount);  // Reserve space for efficiency
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
            ShareType share = deserializeShare(shareStr);
            received.push_back(share);
            count++;
        }
        catch (const std::exception& e) {
            std::cerr << "[Party " << m_partyId << "] Failed to deserialize share from Party " 
                      << senderId << ": " << e.what() << "\n";
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
        ShareType secret = AdditiveSecretSharing::newBigInt();
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

// // Demonstrates secure multiplication using Beaver's Triple
// void Party::doMultiplicationDemo() {
//     // For a real multi-party multiplication, each party would hold:
//     // - A share of X, Y
//     // - A share of Beaver triple (a, b, c)
//     // - Each party computes d = x - a, e = y - b
//     // - Sum d, e among all parties => D, E
//     // - Final share = c + aE + bD + DE
//     // Then reconstruct the final product from all parties’ product shares.
//     // Below is just a placeholder.

//     std::cout << "[Party " << m_partyId
//               << "] doMultiplicationDemo: multi-party Beaver logic not fully implemented.\n";
// }

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
    ShareType secretBn = AdditiveSecretSharing::newBigInt();
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
    ShareType myPartialSumBN = AdditiveSecretSharing::newBigInt();
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
            ShareType shareBN = deserializeShare(shareHex);
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
    ShareType finalSum = AdditiveSecretSharing::newBigInt();
    BN_copy(finalSum, m_myPartialSum);

    int needed = m_totalParties - 1;
    int receivedCount = 0;
    while (receivedCount < needed) {
        PARTY_ID_T senderId;
        char buffer[BUFFER_SIZE];
        size_t bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            std::string pHex(buffer, bytesRead);
            ShareType otherPartial = deserializeShare(pHex);
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

// Implement distributeBeaverTriple
void Party::distributeBeaverTriple()
{
    // **Move the log inside the conditional check**
    #if defined(ENABLE_COUT)
    std::cout << "[Party " << m_partyId << "] Initiating Beaver triple distribution.\n";
    #endif

    // 1) Generate random a, b in [0..prime-1].
    ShareType a = AdditiveSecretSharing::newBigInt();
    ShareType b = AdditiveSecretSharing::newBigInt();
    BN_rand_range(a, AdditiveSecretSharing::getPrime());
    BN_rand_range(b, AdditiveSecretSharing::getPrime());

    // 2) Compute c = a*b mod prime.
    ShareType c = AdditiveSecretSharing::newBigInt();
    BN_mod_mul(c, a, b, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());

    // 3) Make shares for a, b, c
    std::vector<ShareType> aShares, bShares, cShares;
    AdditiveSecretSharing::generateShares(a, m_totalParties, aShares);
    AdditiveSecretSharing::generateShares(b, m_totalParties, bShares);
    AdditiveSecretSharing::generateShares(c, m_totalParties, cShares);
    #if defined(ENABLE_MALICIOUS_SECURITY)
    // Generate MAC shares for a, b, c
    std::vector<ShareType> macAShares, macBShares, macCShares;
    AdditiveSecretSharing::generateMacShares(a, m_global_mac_key, m_totalParties, macAShares);
    AdditiveSecretSharing::generateMacShares(b, m_global_mac_key, m_totalParties, macBShares);
    AdditiveSecretSharing::generateMacShares(c, m_global_mac_key, m_totalParties, macCShares);
    // Generate the shares of global key
    std::vector<ShareType> globalMacKeyShares;
    AdditiveSecretSharing::generateShares(m_global_mac_key, m_totalParties, globalMacKeyShares);
    #endif

    // 4) Broadcast each share to the correct party
    for (PARTY_ID_T pid = 1; pid <= m_totalParties; ++pid) {
        // Serialize shares
        std::string aHex = serializeShare(aShares[pid-1]);
        std::string bHex = serializeShare(bShares[pid-1]);
        std::string cHex = serializeShare(cShares[pid-1]);

        std::string tripleMsg = aHex + "|" + bHex + "|" + cHex;
        #if defined(ENABLE_MALICIOUS_SECURITY)
        // Serialize MAC shares in the triple message
        std::string macAMsg = serializeShare(macAShares[pid-1]);
        std::string macBMsg = serializeShare(macBShares[pid-1]);
        std::string macCMsg = serializeShare(macCShares[pid-1]);
        std::string globalMacKeyMsg = serializeShare(globalMacKeyShares[pid-1]);
        tripleMsg += "|" + macAMsg + "|" + macBMsg + "|" + macCMsg + "|" + globalMacKeyMsg;
        #endif

        // Send shares to other parties
        m_comm->sendTo(pid, tripleMsg.c_str(), tripleMsg.size());
        #ifdef ENABLE_COUT
        std::cout << "[Party " << m_partyId << "] Sent Beaver triple shares to Party " << pid << "\n";
        #endif
        // if (pid == m_partyId) {
        //     // Store shares locally
        //     myTriple.a = AdditiveSecretSharing::cloneBigInt(aShares[pid-1]);
        //     myTriple.b = AdditiveSecretSharing::cloneBigInt(bShares[pid-1]);
        //     myTriple.c = AdditiveSecretSharing::cloneBigInt(cShares[pid-1]);

        //     #ifdef ENABLE_COUT
        //     std::cout << "[Party " << m_partyId << "] Stored own Beaver triple shares.\n";
        //     #endif
        // } else {
        //     // Send shares to other parties
        //     m_comm->sendTo(pid, tripleMsg.c_str(), tripleMsg.size());
        //     #ifdef ENABLE_COUT
        //     std::cout << "[Party " << m_partyId << "] Sent Beaver triple shares to Party " << pid << "\n";
        //     #endif
        // }
    }

    // 5) Clean up
    BN_free(a);
    BN_free(b);
    BN_free(c);
    for (auto bn : aShares) BN_free(bn);
    for (auto bn : bShares) BN_free(bn);
    for (auto bn : cShares) BN_free(bn);
    #if defined(ENABLE_MALICIOUS_SECURITY)
    for (auto bn : macAShares) BN_free(bn);
    for (auto bn : macBShares) BN_free(bn);
    for (auto bn : macCShares) BN_free(bn);
    for (auto bn : globalMacKeyShares) BN_free(bn);
    #endif 
}

// Modify receiveBeaverTriple to ensure it only accepts triples from Party with BeaverTriple
void Party::receiveBeaverTriple()
{

    // Wait for message from Party with Beaver triple
    PARTY_ID_T sender;
    char buffer[BUFFER_SIZE];
    size_t bytesRead = m_comm->receive(sender, buffer, sizeof(buffer));

    #ifdef ENABLE_COUT
    std::cout << "[Party " << m_partyId << "] Received Beaver triple from Party " << sender << "\n";
    #endif

    if (bytesRead == 0) {
        throw std::runtime_error("Failed to receive Beaver triple from Party 1");
    }
   

    std::string tripleMsg(buffer, bytesRead);
    #if defined(ENABLE_MALICIOUS_SECURITY)
    // Parse "aHex|bHex|cHex|macAHex|macBHex|macCHex"
    auto sep1 = tripleMsg.find('|');
    auto sep2 = tripleMsg.find('|', sep1 + 1);
    auto sep3 = tripleMsg.find('|', sep2 + 1);
    auto sep4 = tripleMsg.find('|', sep3 + 1);
    auto sep5 = tripleMsg.find('|', sep4 + 1);
    auto sep6 = tripleMsg.find('|', sep5 + 1);
    if (sep1 == std::string::npos || sep2 == std::string::npos || sep3 == std::string::npos || sep4 == std::string::npos || sep5 == std::string::npos || sep6 == std::string::npos) {
        throw std::runtime_error("Invalid Beaver triple format received");
    }
    std::string aHex = tripleMsg.substr(0, sep1);
    std::string bHex = tripleMsg.substr(sep1 + 1, sep2 - (sep1 + 1));
    std::string cHex = tripleMsg.substr(sep2 + 1, sep3 - (sep2 + 1));
    std::string macAHex = tripleMsg.substr(sep3 + 1, sep4 - (sep3 + 1));
    std::string macBHex = tripleMsg.substr(sep4 + 1, sep5 - (sep4 + 1));
    std::string macCHex = tripleMsg.substr(sep5 + 1);
    std::string globalMacKeyHex = tripleMsg.substr(sep6 + 1);
    #else
    // Parse "aHex|bHex|cHex"
    auto sep1 = tripleMsg.find('|');
    auto sep2 = tripleMsg.rfind('|');
    if (sep1 == std::string::npos || sep2 == std::string::npos || sep1 == sep2) {
        throw std::runtime_error("Invalid triple format received");
    }
    std::string aHex = tripleMsg.substr(0, sep1);
    std::string bHex = tripleMsg.substr(sep1 + 1, sep2 - (sep1 + 1));
    std::string cHex = tripleMsg.substr(sep2 + 1);
    #endif

    // Deserialize shares
    myTriple.a = deserializeShare(aHex);
    myTriple.b = deserializeShare(bHex);
    myTriple.c = deserializeShare(cHex);
    #if defined(ENABLE_MALICIOUS_SECURITY)
    myTripleMac.a = deserializeShare(macAHex);
    myTripleMac.b = deserializeShare(macBHex);
    myTripleMac.c = deserializeShare(macCHex);
    m_global_key_share = deserializeShare(globalMacKeyHex);
    #endif

    #ifdef ENABLE_COUT
    std::cout << "[Party " << m_partyId << "] Successfully received Beaver triple shares.\n";
    #endif
}

// Implement doMultiplicationDemo
void Party::doMultiplicationDemo(ShareType &z_i)
{
    // Compute d_i = x_i - a_i and e_i = y_i - b_i
    ShareType d_i = AdditiveSecretSharing::newBigInt();
    ShareType e_i = AdditiveSecretSharing::newBigInt();
    if(!BN_mod_sub(d_i, m_receivedShares[0], myTriple.a, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx())) {
        throw std::runtime_error("BN_mod_sub failed for d_i");
    }
    if(!BN_mod_sub(e_i, m_receivedShares[1], myTriple.b, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx())) {
        throw std::runtime_error("BN_mod_sub failed for e_i");
    }

    // Broadcast d_i and e_i to all other parties
    std::string dHex = serializeShare(d_i);
    std::string eHex = serializeShare(e_i);
    std::string deMsg = dHex + "|" + eHex;
    // Send to all except self
    for (PARTY_ID_T pid = 1; pid <= m_totalParties; ++pid) {
        if (pid == m_partyId) continue;
        m_comm->sendTo(pid, deMsg.c_str(), deMsg.size());
        #ifdef ENABLE_COUT
        std::cout << "[Party " << m_partyId << "] Sent d_i and e_i to Party " << pid << "\n";
        #endif
    }

    // Receive all d_j and e_j from other parties
    ShareType D = AdditiveSecretSharing::newBigInt();
    ShareType E = AdditiveSecretSharing::newBigInt();
    BN_zero(D);
    BN_zero(E);

    // Add own d_i and e_i
    BN_mod_add(D, D, d_i, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    BN_mod_add(E, E, e_i, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());

    for (int count = 0; count < (m_totalParties - 1); ++count) {
        PARTY_ID_T senderId;
        char buffer[BUFFER_SIZE];
        size_t bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
        if (bytesRead == 0) {
            throw std::runtime_error("Received empty d_j and e_j from Party " + std::to_string(senderId));
        }
        std::string deReceived(buffer, bytesRead);

        auto pos = deReceived.find("|");
        if (pos == std::string::npos) {
            throw std::runtime_error("Invalid d_j|e_j format from Party " + std::to_string(senderId));
        }
        std::string dStr = deReceived.substr(0, pos);
        std::string eStr = deReceived.substr(pos + 1);

        ShareType d_j = deserializeShare(dStr);
        ShareType e_j = deserializeShare(eStr);

        BN_mod_add(D, D, d_j, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
        BN_mod_add(E, E, e_j, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());

        BN_free(d_j);
        BN_free(e_j);
    }
    #if defined(ENABLE_MALICIOUS_SECURITY)
    // Copy the D to m_epsilon and E to m_delta
    BN_copy(m_epsilon, D);
    BN_copy(m_rho, E);
    #endif
    // this->syncAfterDistribute();
    // Compute z_i = c_i + a_i * E + b_i * D + D * E
    // z_i = AdditiveSecretSharing::newBigInt();
    BN_copy(z_i, myTriple.c);

    ShareType aE = AdditiveSecretSharing::newBigInt();
    BN_mod_mul(aE, myTriple.a, E, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    BN_mod_add(z_i, z_i, aE, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());

    ShareType bD = AdditiveSecretSharing::newBigInt();
    BN_mod_mul(bD, myTriple.b, D, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    BN_mod_add(z_i, z_i, bD, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());

    if (m_partyId == 1) {
        ShareType DE = AdditiveSecretSharing::newBigInt();
        BN_mod_mul(DE, D, E, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
        BN_mod_add(z_i, z_i, DE, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
        BN_free(DE);
    }
    
    #if defined(ENABLE_COUT)
    std::cout << "[doMultiplicationDemo][Party " << m_partyId << "] z_i = " << BN_bn2dec(z_i) << "\n";
    #endif

    // Cleanup
    BN_free(d_i);
    BN_free(e_i);
    BN_free(D);
    BN_free(E);
    BN_free(aE);
    BN_free(bD);
}

void Party::runEventLoop()
{
    #ifdef ENABLE_COUT
    std::cout << "[Party " << m_partyId << "] Starting event loop.\n";
    #endif

    while (m_running) {
        PARTY_ID_T senderId;
        char buffer[BUFFER_SIZE];
        size_t bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            #if defined(ENABLE_COUT)
            std::cout << "[Party " << m_partyId << "] Received message from Party " << senderId
                      << ": " << std::string(buffer, bytesRead) << "\n";
            #endif
            // launch a new thread to handle the message
            // std::thread([this, senderId, buffer, bytesRead]() {
            handleMessage(senderId, buffer, bytesRead);
            // }).detach();
            
        } else {
            // std::cerr << "[Party " << m_partyId << "] Received empty message from Party "
            //           << senderId << ".\n";
        }
    }

    // Use the interface's close method instead of just calling destructor
    // m_comm->close();

    #ifdef ENABLE_COUT
    std::cout << "[Party " << m_partyId << "] Event loop stopping.\n";
    #endif
}

void Party::handleMessage(PARTY_ID_T senderId, const void *data, LENGTH_T length){
    // Convert data to CMD_T
    CMD_T cmd;
    std::memcpy(&cmd, data, length);
    if (cmd == CMD_SEND_SHARES) {
        #if defined(ENABLE_UNIT_TESTS)
        std::cout << "[Party " << m_partyId << "] Received command to send shares from Party " 
                  << senderId << "\n";
        #endif // ENABLE_UNIT_TESTS
        // m_dealRouterId = m_comm->getLastRoutingId();
        std::cout << "[Party " << m_partyId << "] has the value of m_lastRoutingId: " << m_comm->getLastRoutingId() << "\n";
        
        // Receive the share string from the sender
        char buffer[BUFFER_SIZE];
        size_t bytesRead = m_comm->receive(senderId, buffer, sizeof(buffer));
        std::cout << "[Party " << m_partyId << "] Received share data from Party " << senderId << "\n";
        if (bytesRead == 0) {
            std::cerr << "[Party " << m_partyId << "] Received empty share data from Party " 
                      << senderId << "\n";
            return;
        }
        std::string shareStr(buffer, bytesRead);

        // Split the received string into individual share hex strings
        std::vector<std::string> shareParts;
        std::stringstream ss(shareStr);
        std::string item;
        while (std::getline(ss, item, '|')) {
            if (!item.empty()) {
                shareParts.push_back(item);
            }
        }
        #if defined(ENABLE_UNIT_TESTS)
        // Verify that the number of received shares matches expected
        #if defined(ENABLE_MALICIOUS_SECURITY)
        if (shareParts.size() != 2 * NUM_SECRETS) {
            std::cerr << "[Party " << m_partyId << "] Expected " << NUM_SECRETS 
                      << " shares but received " << shareParts.size() << " from Party " 
                      << senderId << "\n";
            return;
        }
        #else 
        if (shareParts.size() != NUM_SECRETS) {
            std::cerr << "[Party " << m_partyId << "] Expected " << NUM_SECRETS 
                      << " shares but received " << shareParts.size() << " from Party " 
                      << senderId << "\n";
            return;
        }
        #endif // ENABLE_MALICIOUS_SECURITY
        #endif

        // Deserialize each share hex string into ShareType
        // std::vector<ShareType> receivedShares;
        m_receivedShares.clear();
        #if defined(ENABLE_MALICIOUS_SECURITY)
        m_receivedMacShares.clear();
        #endif
        try {
            // for (const auto& shareHex : shareParts) {
            //     ShareType share = deserializeShare(shareHex);
            //     #if defined(ENABLE_UNIT_TESTS)
            //     std::cout << "[Party " << m_partyId << "] Received share: " << BN_bn2dec(share) << "\n";
            //     #endif
            //     m_receivedShares.push_back(share);
            // }
            for (SIZE_T i = 0; i < shareParts.size(); ++i) {
                ShareType share = deserializeShare(shareParts[i]);
                #if defined(ENABLE_UNIT_TESTS)
                std::cout << "[Party " << m_partyId << "] Received share: " << BN_bn2dec(share) << "\n";
                #endif
                if (i < NUM_SECRETS) {
                    m_receivedShares.push_back(share);
                } else {
                    #if defined(ENABLE_MALICIOUS_SECURITY)
                    m_receivedMacShares.push_back(share);
                    #endif
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[Party " << m_partyId << "] Failed to deserialize shares from Party " 
                      << senderId << ": " << e.what() << "\n";
            return;
        }
        // Acknowledge successful reception
        m_comm->reply(&CMD_SUCCESS, sizeof(CMD_T));
    }
    else if (cmd == CMD_SHUTDOWN) {
        std::cout << "[Party " << m_partyId << "] Received shutdown command from Party " 
                  << senderId << "\n";
        m_running = false;
    } else if (cmd == CMD_ADDITION) {
        #if defined(ENABLE_UNIT_TESTS)
        std::cout << "[Party " << m_partyId << "] Received command to perform addition from Party " 
                  << senderId << "\n";
        #endif // ENABLE_UNIT_TESTS
        // Perform addition with received shares in m_receivedShares
        ShareType sum_result = AdditiveSecretSharing::newBigInt();
        AdditiveSecretSharing::addShares(m_receivedShares, sum_result);
        #if defined(ENABLE_UNIT_TESTS)
        std::cout << "[Party " << m_partyId << "] Sum result: " << BN_bn2dec(sum_result) << "\n";
        #endif // ENABLE_UNIT_TESTS
        // Reply the serialized sum back to the sender
        std::string sumStr = serializeShare(sum_result);
        #if defined(ENABLE_MALICIOUS_SECURITY)
        ShareType mac_result = AdditiveSecretSharing::newBigInt();
        AdditiveSecretSharing::addShares(m_receivedMacShares, mac_result);
        sumStr += "|" + serializeShare(mac_result);
        #endif
        m_comm->reply(sumStr.c_str(), sumStr.size());

        // Clean up
        BN_free(sum_result);
        #if defined(ENABLE_MALICIOUS_SECURITY)
        BN_free(mac_result);
        #endif
    } else if (cmd == CMD_MULTIPLICATION) {
        #if defined(ENABLE_UNIT_TESTS)
        std::cout << "[Party " << m_partyId << "] Received command to perform multiplication from Party " 
                  << senderId << "\n";
        #endif // ENABLE_UNIT_TESTS
        this->receiveBeaverTriple();
        m_comm->reply(&CMD_SUCCESS, sizeof(CMD_T));
        // m_comm->reply(&CMD_SUCCESS, sizeof(CMD_T));
        
        #if defined(ENABLE_UNIT_TESTS)
        // check the received Beaver triple values
        std::cout << "[Party " << m_partyId << "] Received Beaver triple shares:\n";
        std::cout << "  a: " << BN_bn2dec(myTriple.a) << "\n";
        std::cout << "  b: " << BN_bn2dec(myTriple.b) << "\n";
        std::cout << "  c: " << BN_bn2dec(myTriple.c) << "\n";
            #if defined(ENABLE_MALICIOUS_SECURITY)
            std::cout << "  macA: " << BN_bn2dec(myTripleMac.a) << "\n";
            std::cout << "  macB: " << BN_bn2dec(myTripleMac.b) << "\n";
            std::cout << "  macC: " << BN_bn2dec(myTripleMac.c) << "\n";
            std::cout << "  globalMacKeyShare: " << BN_bn2dec(m_global_key_share) << "\n";
            #endif
        #endif // ENABLE_UNIT_TESTS
        this->doMultiplicationDemo(m_z_i);
        #if defined(ENABLE_UNIT_TESTS)
        std::cout << "[Party " << m_partyId << "] m_z_i: " << BN_bn2dec(m_z_i) << "\n";
        #endif // ENABLE_UNIT_TESTS
        #if defined(ENABLE_MALICIOUS_SECURITY)
        this->generateZmac(m_z_i_mac);
        #if defined(ENABLE_UNIT_TESTS)
        std::cout << "[Party " << m_partyId << "] m_z_i_mac: " << BN_bn2dec(m_z_i_mac) << "\n";
        #endif // ENABLE_UNIT_TESTS
        #endif
        // // send success to the dealer
        // std::cout << "m_dealRouterId: " << m_dealRouterId << "\n";
        // // m_comm->reply((void*)m_dealRouterId.c_str(), &CMD_SUCCESS, sizeof(CMD_T));
        m_comm->reply((void*)m_dealRouterId.c_str(), m_dealRouterId.size(), &CMD_SUCCESS, sizeof(CMD_T));
    } else if (cmd == CMD_FETCH_MULT_SHARE) {
        std::cout << "[Party " << m_partyId << "] Received command to fetch multiplication share from Party " 
                  << senderId << "\n";
        m_comm->reply(&CMD_SUCCESS, sizeof(CMD_T));
        std::string zStr = serializeShare(m_z_i);
        m_comm->reply(zStr.c_str(), zStr.size());
        BN_free(m_z_i);
        #if defined(ENABLE_MALICIOUS_SECURITY)
        std::string zMacStr = serializeShare(m_z_i_mac);
        m_comm->reply(zMacStr.c_str(), zMacStr.size());
        BN_free(m_z_i_mac);
        generateBatchZeroShare(m_sigma);
            #if defined(ENABLE_UNIT_TESTS)
            std::cout << "[Party " << m_partyId << "] m_sigma: " << BN_bn2dec(m_sigma) << "\n";
            #endif
        std::string sigmaStr = serializeShare(m_sigma);
        m_comm->reply(sigmaStr.c_str(), sigmaStr.size());
        #endif
    } else {
        std::cerr << "[Party " << m_partyId << "] Unknown command received from Party " 
                  << senderId << ": " << cmd << "\n";
    }
}

// ...existing code...

void Party::generateMyShares(const std::vector<ShareType> &secretValues,
                             std::unordered_map<ShareType, std::vector<ShareType>> &secretSharesMap){
    #if defined(ENABLE_UNIT_TESTS)
    ShareType reconstructed = AdditiveSecretSharing::newBigInt();
    #endif // ENABLE_UNIT_TESTS
    for (ShareType secretBN : secretValues) {
        if (!secretBN) throw std::runtime_error("Secret ShareType is null.");

        // Generate shares
        std::vector<ShareType> shares;
        AdditiveSecretSharing::generateShares(secretBN, m_totalParties, shares);
        #if defined(ENABLE_UNIT_TESTS)
        std::cout << "After generating shares\n";
        for (auto &share : shares) {
            std::cout << "[Party " << m_partyId << "] Share: " << BN_bn2dec(share) << "\n";
        }
        #endif
        // Test the correctness of the shares by reconstructing the secret
        #if defined(ENABLE_UNIT_TESTS)
        AdditiveSecretSharing::reconstructSecret(shares, reconstructed);
        std::cout << "[Party " << m_partyId << "] Reconstruction test for secret " 
                  << BN_bn2dec(secretBN) << ": " << BN_bn2dec(reconstructed) << "\n";
        #endif

        // Store them in the map
        secretSharesMap[secretBN] = std::move(shares);
        std::cout << "After storing shares\n";
        #if defined(ENABLE_UNIT_TESTS)
        // Check the shares stored in the unordered_map
        std::cout << "[Party " << m_partyId << "] Shares for secret " << BN_bn2dec(secretBN) << ":\n";
        for (auto &share : secretSharesMap[secretBN]) {
            std::cout << "  " << BN_bn2dec(share) << "\n";
        }
        #endif

        #ifdef ENABLE_COUT
        std::cout << "[Party " << m_partyId << "] Generated shares for one ShareType secret\n";
        #endif
    }
    #if defined(ENABLE_UNIT_TESTS)
    BN_free(reconstructed);
    #endif // ENABLE_UNIT_TESTS
}

#if defined(ENABLE_MALICIOUS_SECURITY)
void Party::generateZmac(ShareType z_i_mac) {
    // check if z_i_mac is null
    if (!z_i_mac) throw std::runtime_error("z_i_mac is null.");
    BN_zero(z_i_mac);
    BN_mod_add(z_i_mac, z_i_mac, myTripleMac.c, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // m_epsilon * myTripleMac.b
    ShareType temp = AdditiveSecretSharing::newBigInt();
    BN_mod_mul(temp, m_epsilon, myTripleMac.b, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    BN_mod_add(z_i_mac, z_i_mac, temp, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // m_rho * myTripleMac.a
    BN_mod_mul(temp, m_rho, myTripleMac.a, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    BN_mod_add(z_i_mac, z_i_mac, temp, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // m_epsilon * m_rho * m_global_key_share
    BN_mod_mul(temp, m_epsilon, m_rho, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    BN_mod_mul(temp, temp, m_global_key_share, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    BN_mod_add(z_i_mac, z_i_mac, temp, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
}

void Party::generateBatchZeroShare(ShareType zeroShare) {
    // check if zeroShare is null
    if (!zeroShare) throw std::runtime_error("zeroShare is null.");
    BN_zero(zeroShare);
    std::cout << "The value of zeroShare is: " << BN_bn2dec(zeroShare) << "\n";
    ShareType temp = AdditiveSecretSharing::newBigInt();
    // m_receivedMacShares[0] - myTripleMac.a
    BN_mod_sub(temp, m_receivedMacShares[0], myTripleMac.a, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // r_epsilon * (m_receivedMacShares[0] - myTripleMac.a)
    BN_mod_mul(temp, m_agreed_random_values[0], temp, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // zeroShare += temp
    BN_mod_add(zeroShare, zeroShare, temp, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // m_receivedMacShares[1] - myTripleMac.b
    BN_mod_sub(temp, m_receivedMacShares[1], myTripleMac.b, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // r_rho * (m_receivedMacShares[0] - myTripleMac.a)
    BN_mod_mul(temp, m_agreed_random_values[1], temp, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // zeroShare += temp
    BN_mod_add(zeroShare, zeroShare, temp, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // r_epsilon * m_epsilon
    BN_mod_mul(temp, m_agreed_random_values[0], m_epsilon, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // ShareType temp2 = AdditiveSecretSharing::newBigInt() \gets r_rho * m_rho
    ShareType temp2 = AdditiveSecretSharing::newBigInt();
    BN_mod_mul(temp2, m_agreed_random_values[1], m_rho, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // temp += temp2
    BN_mod_add(temp, temp, temp2, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // temp *= m_global_mac_key_share
    BN_mod_mul(temp, temp, m_global_key_share, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // zeroShare -= temp
    BN_mod_sub(zeroShare, zeroShare, temp, AdditiveSecretSharing::getPrime(), AdditiveSecretSharing::getCtx());
    // Free the temporary ShareType
    BN_free(temp2);
    BN_free(temp);
}
#endif