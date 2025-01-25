#ifndef PARTY_H
#define PARTY_H
#include "config.h"

#include <map>
#include <zmq.hpp>
class Party {
public:
    Party(T_PARTY_ID id, T_PARTY_ID numParties, bool hasSecret, std::map<T_PARTY_ID, std::pair<std::string, int>> partyInfo) :
        m_id(id), m_numParties(numParties), m_hasSecret(hasSecret), m_partyInfo(partyInfo) {}


    // Properties
    T_PARTY_ID m_id;
    T_PARTY_ID m_numParties;
    bool m_hasSecret;
    std::map<T_PARTY_ID, std::pair<std::string, int>> m_partyInfo;  
};
#endif // PARTY_H