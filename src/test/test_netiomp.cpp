#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "NetIOMP.h" // Include the custom NetIOMP class

using namespace std;

// const char* IP[] = {
//     "127.0.0.1", // Unused
//     "127.0.0.1", // Party 1
//     "127.0.0.1", // Party 2
//     "127.0.0.1"  // Party 3
// };

#define LOCALHOST // Enable localhost-specific behavior in NetIOMP

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <party_id> <port>\n";
        return 1;
    }

    int party_id = stoi(argv[1]);
    int port = stoi(argv[2]);

    // Initialize NetIOMP with 3 parties
    NetIOMP<3> net(party_id, port);

    // Synchronize all parties
    net.sync();

    // Prepare a message to send
    string my_message = "Hello from party " + to_string(party_id);

    // Send the message to every other party
    for (int p = 1; p <= 3; p++) {
        if (p != party_id) {
            net.send_data(p, my_message.data(), my_message.size());
        }
    }

    // Receive messages from all other parties
    for (int p = 1; p <= 3; p++) {
        if (p != party_id) {
            char buffer[1024] = {0}; // Buffer for received messages
            net.recv_data(p, buffer, 1024);
            cout << "[Party " << party_id << "] Received from party " 
                 << p << ": " << buffer << endl;
        }
    }

    // Final synchronization
    net.sync();

    return 0;
}
