#pragma once
#include "INetIOMP.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <iostream>
#include <thread>   // Add this for std::this_thread
#include <chrono>   // Add this for std::chrono
#include "AdditiveSecretSharing.h" // incorporate big-int sharing
#include <string> // Add this for string operations
#include "config.h" // Include config.h for COUT macro
#include <openssl/bn.h> // Ensure BIGNUM is included

/**
 * @brief Represents an individual party in the MPC protocol.
 */

class Party {
public:
    Party(PARTY_ID_T id, int totalParties, int localValue, INetIOMP* comm,
          bool hasSecret, const std::string& operation)
        : m_partyId(id), m_totalParties(totalParties), m_localValue(localValue),
          m_comm(comm), m_hasSecret(hasSecret), m_operation(operation) {
            // Party5_to_1
            m_dealRouterId = "Party" + std::to_string(m_totalParties + 1) + "_to_" + std::to_string(m_partyId);
            #if defined(ENABLE_MALICIOUS_SECURITY)
            m_macShares.resize(NUM_SECRETS);
            m_global_mac_key = AdditiveSecretSharing::newBigInt();
            m_addition_partial_sum.resize(NUM_TWO);
            for (int i = 0; i < NUM_TWO; ++i) {
                m_addition_partial_sum[i].resize(m_totalParties);
            }
            m_secret_sum = AdditiveSecretSharing::newBigInt();
            m_secret_sum_mac = AdditiveSecretSharing::newBigInt();
            #endif // ENABLE_MALICIOUS_SECURITY
            m_secrets.resize(NUM_SECRETS);
          }
    // Destrcutor to free the BIGNUMs
    ~Party();

    /**
     * @brief Initializes any necessary communication steps (already done in main usually).
     */
    void init();
    void broadcastAllData(const void* data, LENGTH_T length);
    void receiveAllData(void* data, LENGTH_T length);
    /**
     * @brief Sends this party's local value to all other parties.
     */
    void broadcastValue() {
        for (int i = 1; i <= m_totalParties; ++i) {
            if (i != m_partyId) {
                m_comm->sendTo(i, &m_localValue, sizeof(int));
                #ifdef ENABLE_COUT
                std::cout << "[Party " << m_partyId 
                          << "] Sent local value " << m_localValue 
                          << " to Party " << i << "\n";
                #endif
            }
        }
    }

    /**
     * @brief Receives each party's local value and calculates the total sum,
     *        including this party's own value.
     * @return The total sum of all party inputs.
     */
    int receiveAllValuesAndComputeSum() {
        int sum = m_localValue;
        int receivedCount = 0;
        while (receivedCount < (m_totalParties - 1)) {
            PARTY_ID_T senderId;
            int receivedValue;
            m_comm->receive(senderId, &receivedValue, sizeof(receivedValue));
            sum += receivedValue;
            receivedCount++;
            #ifdef ENABLE_COUT
            std::cout << "[Party " << m_partyId 
                      << "] Received value " << receivedValue 
                      << " from Party " << senderId << "\n";
            #endif
        }
        #ifdef ENABLE_COUT
        std::cout << "[Party " << m_partyId 
                  << "] Computed total sum: " << sum << "\n";
        #endif
        return sum;
    }

    void broadcastShares(const std::vector<ShareType> &shares);
    void receiveShares(std::vector<ShareType> &received, int expectedCount);
    void secureMultiplyShares(ShareType myShareX, ShareType myShareY,
                              const BeaverTriple &myTripleShare, ShareType &productOut);

    /**
     * @brief Broadcasts this party's partial sum to all other parties.
     * @param partialSum The computed partial sum of this party's shares.
     */
    void broadcastPartialSum(long long partialSum);

    /**
     * @brief Receives partial sums from all other parties and computes the global sum.
     * @param globalSum Reference to store the final global sum.
     */
    void receivePartialSums(long long& globalSum);

    // Each party’s secret shares for that party’s secret
    std::vector<ShareType> myShares;

    // allShares[pid] holds that party's share for my secret
    // or more generally, allShares[pid][thisParty], if you store a 2D structure
    std::vector<std::vector<ShareType>> allShares;

    // Each party generates shares for its own secret and sends them out
    void distributeOwnShares();

    // Each party receives shares from all other parties
    void gatherAllShares();

    // Reconstruct all secrets from the shares and compute global sum
    void computeGlobalSumOfSecrets();

    // // A placeholder for multi-party multiplication with beaver
    // void doMultiplicationDemo();

    // Declare the new methods
    void distributeSharesAndComputeMyPartial();
    void broadcastAndReconstructGlobalSum();

    // Comment out or remove old global-sharing methods
    // void broadcastAllHeldShares();
    // void gatherAllShares();
    // void computeGlobalSumOfSecrets();
    // void doMultiplicationDemo();

    // A place to store the triple share we receive
    BeaverTriple myTriple;
    // A place to store the triple mac share we receive
    #if defined(ENABLE_MALICIOUS_SECURITY)
    BeaverTriple myTripleMac;
    #endif // ENABLE_MALICIOUS_SECURITY

    // Distribute a random triple [a], [b], [c=a*b] among all parties
    void distributeBeaverTriple();

    // Receive triple shares from the designated "dealer" or from each party
    void receiveBeaverTriple();

    // Perform a single “demo” multiply of (x,y) using the triple
    // and reconstruct final product
    void doMultiplicationDemo(ShareType &z_i);

    // Add these two methods
    void runEventLoop();
    void handleMessage(PARTY_ID_T senderId, const void *data, LENGTH_T length);

    void generateMyShares(const std::vector<ShareType> &secretValues,
                          std::unordered_map<ShareType, std::vector<ShareType>> &secretSharesMap);

private:
    PARTY_ID_T m_partyId;
    int m_totalParties;
    int m_localValue;
    INetIOMP* m_comm;
    BIGNUM* m_myPartialSum = nullptr; // new field for partial sum

    // Add declarations for sync methods
    void syncAfterDistribute();
    void syncAfterGather();

    // For partial opening
    BIGNUM* partialOpen(BIGNUM* share, BIGNUM* tripleShare);

    // Helper to broadcast a single BN as hex
    void broadcastBN(BIGNUM* bn);

    // Helper to receive a single BN from one party
    BIGNUM* receiveBN(PARTY_ID_T& sender);

    bool m_hasSecret;              // Indicates if this party holds a secret
    std::string m_operation;       // "add" or "mul"
    CMD_T m_cmd;
    bool m_running = true;
    std::vector<ShareType> m_receivedShares;
    #if defined(ENABLE_MALICIOUS_SECURITY)
    std::vector<ShareType> m_receivedMacShares;
    #endif // ENABLE_MALICIOUS_SECURITY
    std::vector<ShareType> m_receivedMultiplicationShares;
    ShareType m_z_i;
    // Party5_to_1
    std::string m_dealRouterId;
    ShareType m_global_mac_key;
    #if defined(ENABLE_MALICIOUS_SECURITY)
    std::vector<std::vector<ShareType>> m_macShares;
    // Addition operation
    std::vector<std::vector<ShareType>> m_addition_partial_sum;
    ShareType m_secret_sum;
    ShareType m_secret_sum_mac;
    #endif // ENABLE_MALICIOUS_SECURITY
    std::vector<ShareType> m_secrets;
};