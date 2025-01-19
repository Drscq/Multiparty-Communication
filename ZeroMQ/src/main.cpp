#include "INetIOMP.h"
#include <zmq.hpp> // For ZeroMQ
#include "NetIOMPFactory.h"
#include "config.h"
#include <iostream>
#include <cstring>
#include <cstdlib> // For std::atoi
#include <thread>  // For std::thread
#include <chrono>  // For timing

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

        // Wait for a short period to ensure the server is ready
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (mode == NetIOMPFactory::Mode::REQ_REP) {
            // REQ/REP logic: each request expects an immediate reply

            // Send input value to Party 1 with retry mechanism
            if (myPartyId != 1) {
                // If we're Party 3, delay slightly so Party 2's message arrives first
                if (myPartyId == 3) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                std::cout << "Party " << myPartyId << " sending input value to Party 1\n";
                const int maxRetries = 5;
                int retries = 0;
                while (retries < maxRetries) {
                    try {
                        netIOMP->sendTo(1, &inputValue, sizeof(inputValue));
                        break; // Exit the loop if successful
                    } catch (const zmq::error_t& e) {
                        std::cerr << "Error sending to Party 1: " << e.what() << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait before retrying
                        retries++;
                    }
                }
                if (retries == maxRetries) {
                    throw std::runtime_error("Max retries reached. Failed to send input value to Party 1.");
                }
            }

            // Party 1 receives inputs and computes the sum
            if (myPartyId == 1) {
                int sum = inputValue; // Party 1's own input
                std::map<PARTY_ID_T, int> inputs;  // Store received inputs keyed by party ID

                // Collect input from all other parties
                while (inputs.size() < static_cast<size_t>(totalParties - 1)) {
                    PARTY_ID_T senderId;
                    int receivedValue;
                    netIOMP->receive(senderId, &receivedValue, sizeof(receivedValue));

                    if (senderId != 1 && inputs.find(senderId) == inputs.end()) {
                        inputs[senderId] = receivedValue;
                        sum += receivedValue;
                        std::cout << "Party 1 received input value " << receivedValue 
                                  << " from Party " << senderId << "\n";
                    } else {
                        std::cout << "Party 1 received a duplicate or invalid input from Party " 
                                  << senderId << ", ignoring.\n";
                    }
                    // Send acknowledgment reply
                    std::string ack = "ACK from Party 1";
                    netIOMP->reply(ack.c_str(), static_cast<LENGTH_T>(ack.size()));
                }

                // Now we have inputs from all other parties
                std::cout << "Party 1 computed sum: " << sum << std::endl;

                // Broadcast the result to all other parties
                for (int i = 1; i <= totalParties; ++i) {
                    if (i != 1) {
                        std::cout << "Party " << myPartyId << " sending result to Party " << i << "\n";
                        netIOMP->sendTo(i, &sum, sizeof(sum));
                        std::cout << "Party " << myPartyId << " sent result to Party " << i << "\n";
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            } else {
                // Other parties receive the result
                int result;
                PARTY_ID_T senderId;
                std::cout << "Party " << myPartyId << " waiting to receive result from Party 1\n";
                netIOMP->receive(senderId, &result, sizeof(result));
                std::cout << "Party " << myPartyId << " received result: " << result << " from Party " << senderId << std::endl;
                
                // Send acknowledgment back to Party 1
                std::string ack = "Result received";
                netIOMP->reply(ack.c_str(), static_cast<LENGTH_T>(ack.size()));
            }

        } else {
            // DEALER/ROUTER logic: no forced reply required

            // Send input value to Party 1 with retry mechanism
            if (myPartyId != 1) {
                // // If we're Party 3, delay slightly so Party 2's message arrives first
                // if (myPartyId == 3) {
                //     std::this_thread::sleep_for(std::chrono::seconds(1));
                // }

                std::cout << "Party " << myPartyId << " sending input value to Party 1\n";
                const int maxRetries = 5;
                int retries = 0;
                while (retries < maxRetries) {
                    try {
                        netIOMP->sendTo(1, &inputValue, sizeof(inputValue));
                        break; // Exit the loop if successful
                    } catch (const zmq::error_t& e) {
                        std::cerr << "Error sending to Party 1: " << e.what() << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait before retrying
                        retries++;
                    }
                }
                if (retries == maxRetries) {
                    throw std::runtime_error("Max retries reached. Failed to send input value to Party 1.");
                }
            }

            // Party 1 receives inputs and computes the sum
            if (myPartyId == 1) {
                int sum = inputValue; // Party 1's own input
                std::map<PARTY_ID_T, int> inputs;  // Store received inputs keyed by party ID

                // Collect input from all other parties
                while (inputs.size() < static_cast<size_t>(totalParties - 1)) {
                    PARTY_ID_T senderId;
                    int receivedValue;
                    netIOMP->receive(senderId, &receivedValue, sizeof(receivedValue));

                    if (senderId != 1 && inputs.find(senderId) == inputs.end()) {
                        inputs[senderId] = receivedValue;
                        sum += receivedValue;
                        std::cout << "Party 1 received input value " << receivedValue 
                                  << " from Party " << senderId << "\n";
                    } else {
                        std::cout << "Party 1 received a duplicate or invalid input from Party " 
                                  << senderId << ", ignoring.\n";
                    }

                    // Send a short acknowledgment reply so the dealer can unblock
                    std::string ack = "ACK from Party 1";
                    netIOMP->reply(ack.c_str(), static_cast<LENGTH_T>(ack.size()));
                }

                // Now we have inputs from all other parties
                std::cout << "Party 1 computed sum: " << sum << std::endl;

                // Broadcast the result to all other parties
                for (int i = 1; i <= totalParties; ++i) {
                    if (i != 1) {
                        std::cout << "Party " << myPartyId << " sending result to Party " << i << "\n";
                        netIOMP->sendTo(i, &sum, sizeof(sum));
                        std::cout << "Party " << myPartyId << " sent result to Party " << i << "\n";
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            } else {
                // Other parties receive the result
                int result;
                PARTY_ID_T senderId;
                std::cout << "Party " << myPartyId << " waiting to receive result from Party 1\n";
                netIOMP->receive(senderId, &result, sizeof(result));
                std::cout << "Party " << myPartyId << " received result: " << result << " from Party " << senderId << std::endl;
            }
        }

        // Add small delay before closing to ensure messages are processed
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
