#include "NetIOMPFactory.h"
#include "NetIOMPReqRep.h"
#include "NetIOMPDealerRouter.h"

std::unique_ptr<INetIOMP> NetIOMPFactory::createNetIOMP(Mode mode, PARTY_ID_T partyId, const std::map<PARTY_ID_T, std::pair<std::string, int>>& partyInfo)
{
    switch (mode) {
        case Mode::REQ_REP:
            return std::make_unique<NetIOMPReqRep>(partyId, partyInfo);
        case Mode::DEALER_ROUTER:
            return std::make_unique<NetIOMPDealerRouter>(partyId, partyInfo);
        default:
            throw std::invalid_argument("Unknown NetIOMP mode");
    }
}