#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>  // For fixed-width integer types

// Macro for the type used to store party IDs
#define PARTY_ID_T int16_t

// Use a fixed 32-bit unsigned integer for data lengths
#define LENGTH_T uint32_t

// A 128-bit prime (example, not guaranteed prime):
#define PRIME_128_STR "340282366920938463463374607431768211507"

// Add OpenSSL include for BN_*
#include <openssl/bn.h>

// Add more macros as needed, for example:
// #define PORT_T uint16_t

// Add definition for BUFFER_SIZE if not present
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
#endif

// Uncomment the following line to enable cout statements
// #define ENABLE_COUT

#define ENABLE_UNIT_TESTS

#define CMD_T uint8_t
const CMD_T CMD_SEND_SHARES = 0;
const CMD_T CMD_SUCCESS = 1;
const CMD_T CMD_SHUTDOWN = 2;

#endif // CONFIG_H