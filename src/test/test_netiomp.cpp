#include <emp-tool/emp-tool.h>
#include <iostream>
#include <vector>
#include <string>

// For convenience
using namespace emp;
using namespace std;

/*
 * Usage:
 *   ./test_netiomp <party_id> <party_count> <config_file>
 *
 * Example config_file contents (for 3 parties):
 *   127.0.0.1:12345
 *   127.0.0.1:12346
 *   127.0.0.1:12347
 *
 * Then run in 3 different terminals:
 *   ./test_netiomp 0 3 config.txt
 *   ./test_netiomp 1 3 config.txt
 *   ./test_netiomp 2 3 config.txt
 */

int main(int argc, char** argv) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] 
             << " <party_id> <total_parties> <config_file>\n";
        return 1;
    }

    // Parse command line
    int party_id     = stoi(argv[1]);
    int total_parties = stoi(argv[2]);
    string config_file = argv[3];

    // Read IP addresses and ports from config_file
    vector<string> ips(total_parties);
    vector<int> ports(total_parties);

    // Simple parser for lines in "IP:PORT" format
    {
        ifstream fin(config_file);
        if(!fin.is_open()) {
            cerr << "Error opening config file: " << config_file << endl;
            return 1;
        }
        for(int i = 0; i < total_parties; i++){
            string line;
            if(!getline(fin, line)) {
                cerr << "Not enough lines in config file!\n";
                return 1;
            }
            auto colon_pos = line.find(':');
            if (colon_pos == string::npos) {
                cerr << "Bad line format (need IP:PORT): " << line << endl;
                return 1;
            }
            ips[i] = line.substr(0, colon_pos);
            ports[i] = stoi(line.substr(colon_pos+1));
        }
        fin.close();
    }

    // Create NetIOMP object
    // This automatically creates and manages connections to all other parties
    NetIOMP* net = new NetIOMP(party_id, total_parties, ips, ports);

    // Wait until all parties connect
    // You can do net->sync() if you want a barrier or net->flush() to ensure data is sent
    net->sync();

    // Prepare a message to send
    string my_message = "Hello from party " + to_string(party_id);

    // Send the message to every other party
    for(int p = 0; p < total_parties; p++){
        if(p != party_id){
            net->send_data(p, my_message.data(), my_message.size());
        }
    }

    // Receive from all other parties
    for(int p = 0; p < total_parties; p++){
        if(p != party_id){
            // We don't know how many bytes we will receive, so let's pick a buffer
            // big enough for this demo. In real code, you'd send the length first,
            // or use a known fixed-size message, etc.
            char buffer[1024] = {0};
            net->recv_data(p, buffer, 1024);
            cout << "[Party " << party_id << "] Received from party " 
                 << p << ": " << buffer << endl;
        }
    }

    // One last sync
    net->sync();

    // Clean up
    delete net;
    return 0;
}
