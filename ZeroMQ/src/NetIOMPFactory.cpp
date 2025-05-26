#include "NetIOMPFactory.h"
#include "NetIOMPDealerRouter.h"

std::unique_ptr<INetIOMP> NetIOMPFactory::createNetIOMP(PARTY_ID_T partyId, const std::map<PARTY_ID_T, std::pair<std::string, int>>& partyInfo, int totalParties)
{
    return std::make_unique<NetIOMPDealerRouter>(partyId, partyInfo, totalParties);
}