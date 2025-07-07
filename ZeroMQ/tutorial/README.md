# ZeroMQ Router-Dealer Tutorial

This tutorial demonstrates the ZeroMQ Router-Dealer pattern with practical examples in C++.

## Overview

The Router-Dealer pattern is a powerful messaging pattern in ZeroMQ where:
- **Router**: Acts as a broker that can route messages to multiple dealers
- **Dealer**: Connects to the router and can send/receive messages asynchronously

This pattern is useful for:
- Load balancing workloads across multiple workers
- Building distributed systems with asynchronous communication
- Implementing request-reply patterns with multiple clients

## Project Structure

```
tutorial/
├── CMakeLists.txt                    # Build configuration
├── README.md                         # This file
├── build.sh                          # Convenient build script
├── test_communication.sh             # Test script for basic example
├── router_dealer_basic.cpp           # Simple Router-Dealer example
├── router_dealer_official.cpp        # Official ZeroMQ example with workers
├── zhelpers.hpp                      # ZeroMQ helper functions
└── build/                            # Build output directory
    ├── router_dealer_basic           # Basic example executable
    └── router_dealer_official        # Official example executable
```

## Prerequisites

- C++17 compatible compiler
- CMake 3.15 or later
- ZeroMQ library
- cppzmq (ZeroMQ C++ bindings)

### macOS Installation

```bash
# Install dependencies via Homebrew
brew install cmake zeromq cppzmq
```

### Ubuntu/Debian Installation

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install cmake libzmq3-dev libcppzmq-dev
```

## Building the Project

### Option 1: Using the build script (recommended)

```bash
./build.sh
```

### Option 2: Using CMake directly

```bash
mkdir -p build
cd build
cmake ..
make
```

## Examples

### 1. Basic Router-Dealer Example

**File:** `router_dealer_basic.cpp`

A simple example demonstrating basic Router-Dealer communication:

#### Usage

**Start the router:**
```bash
cd build
./router_dealer_basic router
```

**In another terminal, run the dealer:**
```bash
cd build
./router_dealer_basic dealer
```

**Or use the test script:**
```bash
./test_communication.sh
```

#### What it demonstrates:
- Basic message exchange between router and dealer
- Identity frame handling
- Synchronous request-reply pattern

### 2. Official ZeroMQ Example

**File:** `router_dealer_official.cpp`

A more complex example with multiple worker threads:

#### Usage

```bash
cd build
./router_dealer_official
```

#### What it demonstrates:
- Multiple dealer workers running in parallel
- Load balancing across workers
- Asynchronous message processing
- Worker lifecycle management (hiring and firing)

## Key Concepts

### Router Socket
- Automatically prefixes messages with identity frames
- Can route messages to specific dealers
- Manages multiple connections

### Dealer Socket
- Connects to routers
- Can set custom identity
- Handles asynchronous messaging

### Message Frames
In Router-Dealer communication:
1. **Identity frame**: Automatically added by the dealer
2. **Message payload**: The actual data being sent

## Code Examples

### Basic Router Code
```cpp
zmq::socket_t router{ctx, zmq::socket_type::router};
router.bind("tcp://*:5555");

// Receive message
zmq::message_t identity, payload;
router.recv(identity, zmq::recv_flags::none);
router.recv(payload, zmq::recv_flags::none);

// Send reply
router.send(identity, zmq::send_flags::sndmore);
router.send(zmq::buffer("Reply"), zmq::send_flags::none);
```

### Basic Dealer Code
```cpp
zmq::socket_t dealer{ctx, zmq::socket_type::dealer};
dealer.set(zmq::sockopt::routing_id, "client1");
dealer.connect("tcp://localhost:5555");

// Send message
dealer.send(zmq::buffer("Hello"), zmq::send_flags::none);

// Receive reply
zmq::message_t reply;
dealer.recv(reply, zmq::recv_flags::none);
```

## Testing

### Automated Testing
```bash
# Test basic router-dealer communication
./test_communication.sh
```

### Manual Testing
```bash
# Terminal 1: Start router
cd build && ./router_dealer_basic router

# Terminal 2: Run dealer
cd build && ./router_dealer_basic dealer

# Terminal 3: Run another dealer
cd build && ./router_dealer_basic dealer
```

## Troubleshooting

### Common Issues

1. **"Address already in use" error**
   - Kill existing processes: `pkill -f router_dealer`
   - Wait a few seconds and try again

2. **"No such file or directory" error**
   - Make sure you've built the project first
   - Check that you're in the correct directory

3. **Compilation errors**
   - Ensure ZeroMQ and cppzmq are installed
   - Check CMake output for missing dependencies

### Debug Tips

- Use `netstat -an | grep 5555` to check if the port is in use
- Add debug output to see message flow
- Use ZeroMQ's built-in monitoring capabilities

## Advanced Usage

### Multiple Dealers
You can run multiple dealers simultaneously to test load balancing:

```bash
# Start router
./build/router_dealer_basic router &

# Start multiple dealers
for i in {1..5}; do
    ./build/router_dealer_basic dealer &
done
```

### Custom Identities
Modify the dealer code to use custom identities:

```cpp
dealer.set(zmq::sockopt::routing_id, "custom_client_" + std::to_string(id));
```

## Performance Considerations

- ZeroMQ handles thousands of connections efficiently
- Router-Dealer pattern is ideal for high-throughput scenarios
- Consider using connection pooling for better performance
- Monitor memory usage with many concurrent connections

## References

- [ZeroMQ Guide](https://zguide.zeromq.org/)
- [ZeroMQ C++ Bindings](https://github.com/zeromq/cppzmq)
- [Router-Dealer Pattern Documentation](https://zguide.zeromq.org/docs/chapter3/#The-Router-Dealer-Combination)

## License

This tutorial is provided as educational material. Please refer to ZeroMQ's license for the underlying library usage.
