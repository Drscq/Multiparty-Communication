# Multiparty Communication with ZeroMQ

This project demonstrates a multi-party communication system using ZeroMQ with a Request-Reply (REQ/REP) pattern. The system supports binary data transmission and identifies the sender's party ID.

## Prerequisites

- C++17 compatible compiler (e.g., g++)
- ZeroMQ library
- CMake (optional, if you prefer using CMake for building)

## Project Structure

```
Multiparty-Communication/
├── .gitignore
├── README.md
├── ZeroMQ/
│   ├── Makefile
│   ├── src/
│   │   ├── config.h
│   │   ├── main.cpp
│   │   ├── NetIOMP.cpp
│   │   └── NetIOMP.h
```

## Building the Project

### Using Makefile

1. Navigate to the `ZeroMQ` directory:
    ```sh
    cd ZeroMQ
    ```

2. Build the project using `make`:
    ```sh
    make
    ```

3. The executable will be generated with the name specified in the `Makefile` (e.g., `netiomp_test`).

### Using g++

Alternatively, you can compile the project directly using `g++`:

```sh
g++ -std=c++17 -o netiomp_test src/main.cpp src/NetIOMP.cpp -lzmq -lpthread
```

## Running the Program

1. Run the program with a party ID as an argument:
    ```sh
    ./netiomp_test <party_id>
    ```

    For example:
    ```sh
    ./netiomp_test 1
    ```

2. The program will initialize and be ready to receive and send messages. You can type commands to interact with other parties:
    - `send <target_id> <message>`: Send a message to the specified target party ID.
    - `exit`: Quit the program.

## Example Usage

1. Open three terminal windows and run the program with different party IDs:
    ```sh
    ./netiomp_test 1
    ./netiomp_test 2
    ./netiomp_test 3
    ```

2. In each terminal, you can send messages to other parties. For example, in the terminal for party 1:
    ```sh
    send 2 Hello Party 2
    ```

3. Party 2 will receive the message and reply back.

## Cleaning Up

To clean up the build files, run:
```sh
make clean
```

## License

This project is licensed under the MIT License.# Multiparty-Communication