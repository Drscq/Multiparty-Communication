#include "NetIOMP.h"
#include "config.h"
#include <iostream>
#include <cstring>
#include <cstdlib> // For std::atoi
#include <sstream> // For std::istringstream
#include <thread> // For std::thread

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <party_id>" << std::endl;
        return 1;
    }

    PARTY_ID_T myPartyId = static_cast<PARTY_ID_T>(std::atoi(argv[1]));

    // Example usage: three parties on different ports
    std::map<PARTY_ID_T, std::pair<std::string, int>> partyInfo = {
        {static_cast<PARTY_ID_T>(1), {"127.0.0.1", 5555}},
        {static_cast<PARTY_ID_T>(2), {"127.0.0.1", 5556}},
        {static_cast<PARTY_ID_T>(3), {"127.0.0.1", 5557}}
    };

    NetIOMP netIOMP(myPartyId, partyInfo);
    netIOMP.init();

    std::cout << "Party " << myPartyId
              << " initialized. Ready to receive and send messages.\n"
              << "Type 'send <target> <message>' to send or 'exit' to quit.\n";

    std::thread serverThread([&netIOMP]() {
        netIOMP.runServer();
    });

    while (true) {
        // Prompt for user input each iteration
        std::cout << "> ";
        std::string command;
        if (!std::getline(std::cin, command)) {
            break; // Exit if input stream is lost
        }

        if (command == "exit") {
            break;
        } else if (command.rfind("send ", 0) == 0) {
            std::istringstream iss(command);
            std::string sendKeyword;
            PARTY_ID_T targetId;
            std::string message;

            iss >> sendKeyword >> targetId;
            std::getline(iss, message);
            // Trim leading space
            if (!message.empty() && message[0] == ' ')
                message.erase(0, 1);

            if (!message.empty()) {
                std::cout << "Party " << myPartyId << " sending '"
                          << message << "' to Party " << targetId << std::endl;
                netIOMP.sendTo(targetId, message.c_str(),
                               static_cast<LENGTH_T>(message.size()));
            }
        }
    }

    netIOMP.close();
    serverThread.join();
    std::cout << "Party " << myPartyId << " closed.\n";
    return 0;
}
