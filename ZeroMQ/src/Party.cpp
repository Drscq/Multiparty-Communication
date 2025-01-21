#include "Party.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

// Constructor implementation
Party::Party(PARTY_ID_T id, 
             int totalParties, 
             int localValue, 
             INetIOMP* comm, 
             NetIOMPFactory::Mode mode)
    : m_partyId(id), 
      m_totalParties(totalParties), 
      m_localValue(localValue), 
      m_comm(comm), 
      m_mode(mode)
{
    if (!m_comm) {
        throw std::invalid_argument("INetIOMP pointer cannot be null.");
    }
}

// Initialize method
void Party::init()
{
    // Optionally perform additional initialization
    std::cout << "[Party " << m_partyId << "] Initialization complete.\n";
}

// BroadcastValue method with mode-specific logic
void Party::broadcastValue()
{
    if (m_mode == NetIOMPFactory::Mode::REQ_REP)
    {
        // Round-based broadcast to avoid deadlock
        for (int round = 1; round <= m_totalParties; ++round) {
            if (m_partyId == round) {
                // It's my turn to send to everyone else
                for (int pid = 1; pid <= m_totalParties; ++pid) {
                    if (pid == m_partyId) continue; // Skip sending to self
                    m_comm->sendTo(pid, &m_localValue, sizeof(m_localValue));
                    std::cout << "[Party " << m_partyId 
                              << "] Sent local value " << m_localValue 
                              << " to Party " << pid << " in round " << round << ".\n";
                }
            }
            else {
                // Not my turn; expect exactly one message
                PARTY_ID_T senderId;
                int theirValue = 0;
                size_t bytes = m_comm->receive(senderId, &theirValue, sizeof(theirValue));
                if (bytes != sizeof(theirValue)) {
                    throw std::runtime_error("Invalid message size in REQ/REP broadcastValue().");
                }
                m_comm->reply("OK", 2); // Acknowledge the message
                m_receivedValues[senderId] = theirValue;
                std::cout << "[Party " << m_partyId 
                          << "] Received value " << theirValue 
                          << " from Party " << senderId 
                          << " in round " << round << ".\n";
            }

            // Small delay to ensure message flow
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Store own value
        m_receivedValues[m_partyId] = m_localValue;
    }
    else
    {
        // DEALER/ROUTER mode: send to all other parties asynchronously
        for (int pid = 1; pid <= m_totalParties; ++pid) {
            if (pid == m_partyId) continue; // Skip sending to self
            m_comm->sendTo(pid, &m_localValue, sizeof(m_localValue));
            std::cout << "[Party " << m_partyId 
                      << "] Sent local value " << m_localValue 
                      << " to Party " << pid << " in DEALER/ROUTER mode.\n";
        }
    }

    std::cout << "[Party " << m_partyId << "] Finished broadcastValue() in " 
              << (m_mode == NetIOMPFactory::Mode::REQ_REP ? "REQ/REP" : "DEALER/ROUTER") 
              << " mode.\n";
}

// ReceiveAllValuesAndComputeSum method with mode-specific logic
int Party::receiveAllValuesAndComputeSum()
{
    if (m_mode == NetIOMPFactory::Mode::REQ_REP)
    {
        // For REQ/REP mode, values have been received during broadcast rounds
        // Include own value in the sum
        m_receivedValues[m_partyId] = m_localValue;

        int totalSum = 0;
        for (const auto& [pid, val] : m_receivedValues) {
            totalSum += val;
        }
        std::cout << "[Party " << m_partyId << "] Computed total sum: " << totalSum << "\n";
        return totalSum;
    }
    else
    {
        // DEALER/ROUTER mode: receive (totalParties - 1) messages
        for (int i = 1; i < m_totalParties; ++i) {
            PARTY_ID_T senderId;
            int theirValue = 0;
            size_t bytes = m_comm->receive(senderId, &theirValue, sizeof(theirValue));
            if (bytes != sizeof(theirValue)) {
                throw std::runtime_error("Invalid message size in DEALER/ROUTER receiveAllValuesAndComputeSum().");
            }
            m_receivedValues[senderId] = theirValue;
            std::cout << "[Party " << m_partyId 
                      << "] Received value " << theirValue 
                      << " from Party " << senderId 
                      << " in DEALER/ROUTER mode.\n";
        }

        // Include own value in the sum
        m_receivedValues[m_partyId] = m_localValue;

        // Calculate the total sum
        int totalSum = 0;
        for (const auto& [pid, val] : m_receivedValues) {
            totalSum += val;
        }
        std::cout << "[Party " << m_partyId << "] Computed total sum: " << totalSum << "\n";
        return totalSum;
    }
}