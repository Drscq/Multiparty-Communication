#include "NetIOMP.h"
#include "config.h"
#include <iostream>
#include <cstring>
#include <cstdlib> // For std::atoi
#include <thread>  // For std::thread
#include <chrono>  // For timing

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <party_id> <input_value>" << std::endl;
        return 1;
    }

    PARTY_ID_T myPartyId = static_cast<PARTY_ID_T>(std::atoi(argv[1]));
    int inputValue = std::atoi(argv[2]);

    // Example usage: three parties on different ports
    std::map<PARTY_ID_T, std::pair<std::string, int>> partyInfo = {
        {static_cast<PARTY_ID_T>(1), {"127.0.0.1", 5555}},
        {static_cast<PARTY_ID_T>(2), {"127.0.0.1", 5556}},
        {static_cast<PARTY_ID_T>(3), {"127.0.0.1", 5557}}
    };

    try {
        NetIOMP netIOMP(myPartyId, partyInfo);
        netIOMP.init();

        std::cout << "Party " << myPartyId
                  << " initialized. Ready to perform MPC.\n";

        // Wait for a short period to ensure the server is ready
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Send input value to Party 1 with retry mechanism
        if (myPartyId != 1) {
            std::cout << "Party " << myPartyId << " sending input value to Party 1\n";
            const int maxRetries = 5;
            int retries = 0;
            while (retries < maxRetries) {
                try {
                    netIOMP.sendTo(1, &inputValue, sizeof(inputValue));
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
            int sum = inputValue; // Include Party 1's own input
            for (int i = 2; i <= 3; ++i) {
                int receivedValue;
                PARTY_ID_T senderId;
                std::cout << "Party 1 waiting to receive input from Party " << i << "\n";
                netIOMP.receive(senderId, &receivedValue, sizeof(receivedValue));
                std::cout << "Party 1 received input value " << receivedValue << " from Party " << senderId << "\n";
                
                // Send acknowledgment reply immediately after receiving
                std::string ack = "ACK from Party 1";
                netIOMP.reply(ack.c_str(), static_cast<LENGTH_T>(ack.size()));
                
                sum += receivedValue;
            }

            std::cout << "Party 1 computed sum: " << sum << std::endl;

            // Broadcast the result to all parties
            for (int i = 2; i <= 3; ++i) {
                std::cout << "Party " << myPartyId << " sending result to Party " << i << "\n";
                netIOMP.sendTo(i, &sum, sizeof(sum));
                std::cout << "Party " << myPartyId << " sent result to Party " << i << "\n";
                
                // Add small delay between sends to ensure proper message handling
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } else {
            // Other parties receive the result
            int result;
            PARTY_ID_T senderId;
            std::cout << "Party " << myPartyId << " waiting to receive result from Party 1\n";
            netIOMP.receive(senderId, &result, sizeof(result));
            std::cout << "Party " << myPartyId << " received result: " << result << " from Party " << senderId << std::endl;
            
            // Send acknowledgment back to Party 1
            std::string ack = "Result received";
            netIOMP.reply(ack.c_str(), static_cast<LENGTH_T>(ack.size()));
        }

        // Add small delay before closing to ensure messages are processed
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        netIOMP.close();
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
