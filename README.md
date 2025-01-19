# Multiparty Communication Framework

This project implements a flexible multiparty communication framework using ZeroMQ, supporting both synchronous (REQ/REP) and asynchronous (DEALER/ROUTER) communication patterns for secure multiparty computation (MPC).

## Features

- Support for arbitrary number of parties (n ≥ 2)
- Two communication patterns:
  - REQ/REP: Synchronous request-reply pattern
  - DEALER/ROUTER: Asynchronous messaging pattern with concurrent message handling
- Dynamic port allocation
- Automatic party discovery
- Built-in error handling and retry mechanisms
- Message acknowledgment system

## Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y g++ make libzmq3-dev

# macOS
brew install zeromq
```

## Project Structure

```
Multiparty-Communication/
├── ZeroMQ/
│   ├── src/
│   │   ├── INetIOMP.h              # Interface definition
│   │   ├── NetIOMPDealerRouter.h   # DEALER/ROUTER implementation
│   │   ├── NetIOMPDealerRouter.cpp
│   │   ├── NetIOMPReqRep.h         # REQ/REP implementation
│   │   ├── NetIOMPReqRep.cpp
│   │   ├── NetIOMPFactory.h        # Factory pattern implementation
│   │   ├── NetIOMPFactory.cpp
│   │   ├── config.h                # Configuration constants
│   │   └── main.cpp                # Example implementation
│   ├── Makefile
│   └── run_parties.sh              # Automated test script
```

## Building

```bash
cd ZeroMQ
make clean && make
```

## Running the Example

Two ways to run the example:

1. Using the automated script:
```bash
# Run with default 3 parties
./run_parties.sh dealerrouter

# Run with custom number of parties (e.g., 5)
./run_parties.sh dealerrouter 5
```

2. Manual execution:
```bash
# Format: ./netiomp_test <mode> <party_id> <num_parties> <input_value>
# Terminal 1:
./netiomp_test dealerrouter 1 3 10

# Terminal 2:
./netiomp_test dealerrouter 2 3 20

# Terminal 3:
./netiomp_test dealerrouter 3 3 30
```

## Example Operation

1. Each party initializes with:
   - Unique party ID
   - Total number of parties
   - Input value
   - Communication mode (REQ/REP or DEALER/ROUTER)

2. Operation flow:
   - Party 1 acts as the central coordinator
   - Other parties send their input values to Party 1
   - Party 1 computes the sum of all inputs
   - Party 1 broadcasts the result to all other parties

3. Communication patterns:
   - REQ/REP: Synchronous, with forced acknowledgments
   - DEALER/ROUTER: Asynchronous, supports concurrent messages

## Implementation Details

- Uses ZeroMQ's DEALER/ROUTER pattern for asynchronous communication
- Dynamic port allocation starting from base port 5555
- Automatic retry mechanism for failed connections
- Efficient message routing using party IDs
- Thread-safe message handling
- Proper cleanup of resources

## Error Handling

The framework includes comprehensive error handling for:
- Connection failures
- Message transmission errors
- Invalid party IDs
- Port conflicts
- Resource cleanup

## Limitations

- Currently supports localhost testing only
- Party 1 must be started first
- Basic arithmetic operation (sum) for demonstration
- Maximum message size limited by available memory

## Contributing

Feel free to submit issues and enhancement requests!

## License

MIT License