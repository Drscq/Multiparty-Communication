#pragma once
#include "INetIOMP.h"
#include <vector>
#include <map>
#include <iostream>

/**
 * @brief Represents an individual party in the MPC protocol.
 */
class Party {
public:
    Party(PARTY_ID_T id, int totalParties, int localValue, INetIOMP* comm)
        : m_partyId(id), m_totalParties(totalParties), m_localValue(localValue), m_comm(comm) {}

    /**
     * @brief Initializes any necessary communication steps (already done in main usually).
     */
    void init() {
        // Optionally do extra setup here
        std::cout << "[Party " << m_partyId << "] init called.\n";
    }

    /**
     * @brief Sends this party's local value to all other parties.
     */
    void broadcastValue() {
        for (int i = 1; i <= m_totalParties; ++i) {
            if (i != m_partyId) {
                m_comm->sendTo(i, &m_localValue, sizeof(int));
                std::cout << "[Party " << m_partyId 
                          << "] Sent local value " << m_localValue 
                          << " to Party " << i << "\n";
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
            std::cout << "[Party " << m_partyId 
                      << "] Received value " << receivedValue 
                      << " from Party " << senderId << "\n";
        }
        std::cout << "[Party " << m_partyId 
                  << "] Computed total sum: " << sum << "\n";
        return sum;
    }

private:
    PARTY_ID_T m_partyId;
    int m_totalParties;
    int m_localValue;
    INetIOMP* m_comm;
};