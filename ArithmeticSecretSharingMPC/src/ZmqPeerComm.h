#ifndef ZMQ_PEER_COMM_HPP
#define ZMQ_PEER_COMM_HPP
#include <zmq.hpp>
#include <map>
#include "config.h"
/**
 * @brief A communication layer that encapsulates two sockets:
 * 1) A Router socket (bind) for inbound connections.
 * 2) A Dealer socket (connect) for outbound connections.
 */ 
class ZmqPeerComm {
public:
    ZmqPeerComm(T_PARTY_ID partyId, const std::map<T_PARTY_ID, std::pair<std::string, int>>& partyInfo, T_PARTY_ID totalParties);

};

#endif // ZMQ_PEER_COMM_HPP