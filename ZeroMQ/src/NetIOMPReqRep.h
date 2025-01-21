#ifndef NET_IOMP_REQREP_H
#define NET_IOMP_REQREP_H

#include "INetIOMP.h"
#include <zmq.hpp>
#include <map>
#include <memory>
#include <stdexcept>

/**
 * @brief Implementation of INetIOMP using REQ/REP sockets.
 */
class NetIOMPReqRep : public INetIOMP
{
public:
    NetIOMPReqRep(PARTY_ID_T partyId, const std::map<PARTY_ID_T, std::pair<std::string, int>>& partyInfo);
    void init() override;
    void sendTo(PARTY_ID_T targetId, const void* data, LENGTH_T length) override;
    size_t receive(PARTY_ID_T& senderId, void* buffer, LENGTH_T maxLength) override;
    void reply(const void* data, LENGTH_T length) override;
    void close() override;
    void sendToAll(const void* data, LENGTH_T length) override;
    ~NetIOMPReqRep() override;

private:
    PARTY_ID_T m_partyId;
    std::map<PARTY_ID_T, std::pair<std::string, int>> m_partyInfo;

    zmq::context_t m_context;
    std::unique_ptr<zmq::socket_t> m_repSocket;
    std::map<PARTY_ID_T, std::unique_ptr<zmq::socket_t>> m_reqSockets;
};

#endif // NET_IOMP_REQREP_H