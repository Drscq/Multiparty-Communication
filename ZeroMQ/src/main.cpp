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

        std::cout << "Party " << myPartyId
                  << " initialized. Ready to perform MPC using "
                  << (mode == NetIOMPFactory::Mode::REQ_REP ? "REQ/REP" : "DEALER/ROUTER")
                  << " mode.\n";

        // Create a Party object with our chosen ID, total parties, input value, and netIOMP
        Party myParty(myPartyId, totalParties, inputValue, netIOMP.get()); // Use .get() to pass raw pointer
        myParty.init();

        // 1) Distribute shares for local secret
        myParty.distributeOwnShares();

        // 2) Wait or sync to ensure all parties have distributed
        // std::this_thread::sleep_for(std::chrono::seconds(2));

        // 3) Gather shares from all parties
        myParty.gatherAllShares();

        // 4) Reconstruct global sum
        myParty.computeGlobalSumOfSecrets();

        // 5) (Optional) Show a multiplication example placeholder
        myParty.doMultiplicationDemo();

        netIOMP->close();
        std::cout << "Party " << myPartyId << " closed.\n";
    }
    catch (const zmq::error_t& e) {
        std::cerr << "ZeroMQ Error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Standard Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
