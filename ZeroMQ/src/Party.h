#pragma once
#include "INetIOMP.h"
#include "NetIOMPFactory.h" // Include for Mode enum
#include <vector>
#include <map>
#include <iostream>

/**
 * @brief Represents an individual party in the MPC protocol.
 */
class Party {
public:
    // Constructor declaration
    Party(PARTY_ID_T id, 
          int totalParties, 
          int localValue, 
          INetIOMP* comm, 
          NetIOMPFactory::Mode mode);

    // Method declarations
    void init();
    void broadcastValue();
    int receiveAllValuesAndComputeSum();

private:
    PARTY_ID_T m_partyId;
    int m_totalParties;
    int m_localValue;
    INetIOMP* m_comm;
    NetIOMPFactory::Mode m_mode; // Added mode field

    std::map<PARTY_ID_T, int> m_receivedValues;
};