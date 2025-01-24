#ifndef NET_IOMP_FACTORY_H
#define NET_IOMP_FACTORY_H

#include "INetIOMP.h"
#include <map>
#include <string>
#include <memory>
#include "config.h"

/**
 * @brief Factory class to create instances of INetIOMP implementations.
 */
class NetIOMPFactory
{
public:
    /**
     * @brief Enum to specify the communication mode.
     */
    enum class Mode
    {
        REQ_REP,
        DEALER_ROUTER
    };

    /**
     * @brief Creates an instance of INetIOMP based on the specified mode.
     * @param mode       The communication mode to use.
     * @param partyId    The ID of this party.
     * @param partyInfo  A mapping from party ID -> (ip, port).
     * @return A unique pointer to an INetIOMP instance.
     */
    static std::unique_ptr<INetIOMP> createNetIOMP(Mode mode,
                                                   PARTY_ID_T partyId,
                                                   const std::map<PARTY_ID_T, std::pair<std::string, int>>& partyInfo, int totalParties);
};

#endif // NET_IOMP_FACTORY_H