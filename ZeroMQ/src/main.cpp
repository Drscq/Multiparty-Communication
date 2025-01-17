#include "NetIOMP.h"
#include "config.h"
#include <iostream>
#include <cstring>
#include <cstdlib> // For std::atoi

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <party_id> <message>" << std::endl;
        return 1;
    }

    // Parse command-line arguments
    PARTY_ID_T myPartyId = static_cast<PARTY_ID_T>(std::atoi(argv[1])); // Party ID
    std::string messageToSend = argv[2]; // Message to send

    // Use PARTY_ID_T for the key type
    std::map<PARTY_ID_T, std::pair<std::string, int>> partyInfo = {
        {static_cast<PARTY_ID_T>(1), {"127.0.0.1", 5555}},
        {static_cast<PARTY_ID_T>(2), {"127.0.0.1", 5556}}
    };

    // Initialize NetIOMP
    NetIOMP netIOMP(myPartyId, partyInfo);
    netIOMP.init();

    // Flags to track when each party is done
    bool sentMessage = false;
    bool receivedReply = false;

    // Party 1 sends a message to Party 2
    if (myPartyId == 1) {
        std::cout << "Party " << myPartyId << " sending: '" << messageToSend << "' to Party 2" << std::endl;
        netIOMP.sendTo(static_cast<PARTY_ID_T>(2), messageToSend.c_str(), static_cast<LENGTH_T>(messageToSend.size()));
        sentMessage = true;
    }

    // Keep polling for messages until we've sent and received
    while (!sentMessage || !receivedReply) {
        // pollAndProcess returns true if a message was received
        if (netIOMP.pollAndProcess(1000)) {
            // We've received something. If this is Party 2, we just replied;
            // if it's Party 1, we just got our reply
            receivedReply = true;

            // Party 2 can also send a message back if needed, or set sentMessage
            // if it explicitly needs to send something else.
            if (myPartyId == 2 && !sentMessage) {
                std::cout << "Party 2 sending: '" << messageToSend << "' to Party 1" << std::endl;
                netIOMP.sendTo(static_cast<PARTY_ID_T>(1), messageToSend.c_str(), static_cast<LENGTH_T>(messageToSend.size()));
                sentMessage = true;
            }
        }
    }

    // Once both sending and receiving are complete, close
    netIOMP.close();

    return 0;
}
