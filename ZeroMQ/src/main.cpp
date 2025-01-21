#include "INetIOMP.h"
#include <zmq.hpp> // For ZeroMQ
#include "NetIOMPFactory.h"
#include "config.h"
#include <iostream>
#include <cstring>
#include <cstdlib> // For std::atoi
#include <thread>  // For std::thread
#include <chrono>  // For timing
#include "Party.h" // Add this include for the Party class

int main(int argc, char* argv[])
{
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <mode> <party_id> <num_parties> <input_value>" << std::endl;
        std::cerr << "Modes: reqrep, dealerrouter" << std::endl;
        return 1;
    }

    std::string modeStr = argv[1];
    PARTY_ID_T myPartyId = static_cast<PARTY_ID_T>(std::atoi(argv[2]));
    int totalParties = std::atoi(argv[3]);
    int inputValue = std::atoi(argv[4]);

    // Build the party info map dynamically
    std::map<PARTY_ID_T, std::pair<std::string, int>> partyInfo;
    int basePort = 5555;
    for (int i = 1; i <= totalParties; ++i) {
        partyInfo[static_cast<PARTY_ID_T>(i)] = {"127.0.0.1", basePort + i - 1};
    }

    // Determine the mode
    NetIOMPFactory::Mode mode;
    if (modeStr == "reqrep") {
        mode = NetIOMPFactory::Mode::REQ_REP;
    } else if (modeStr == "dealerrouter") {
        mode = NetIOMPFactory::Mode::DEALER_ROUTER;
    } else {
        std::cerr << "Unknown mode: " << modeStr << std::endl;
        return 1;
    }

    try {
        auto netIOMP = NetIOMPFactory::createNetIOMP(mode, myPartyId, partyInfo);
        netIOMP->init();

        // Ensure all parties are initialized
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        Party myParty(myPartyId, totalParties, inputValue, netIOMP.get());
        myParty.init();

        // Step 1: Generate and distribute shares
        std::cout << "[Party " << myPartyId << "] Starting share distribution\n";
        myParty.distributeOwnShares();

        // Ensure all distributions complete before gathering
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Step 2: Gather shares from other parties
        std::cout << "[Party " << myPartyId << "] Starting share gathering\n";
        myParty.gatherAllShares();

        // Compute global sum after gathering
        myParty.computeGlobalSumOfSecrets();

        // Ensure clean shutdown
        std::this_thread::sleep_for(std::chrono::seconds(1));
        netIOMP->close();
        std::cout << "[Party " << myPartyId << "] Completed successfully\n";
    }
    catch (const zmq::error_t& e) {
        std::cerr << "ZeroMQ Error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "[Party " << myPartyId << "] Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
