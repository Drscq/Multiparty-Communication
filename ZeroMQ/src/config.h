#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>  // For fixed-width integer types

// Macro for the type used to store party IDs
#define PARTY_ID_T int16_t

// Use a fixed 32-bit unsigned integer for data lengths
#define LENGTH_T uint32_t

// Add more macros as needed, for example:
// #define PORT_T uint16_t
// #define BUFFER_SIZE 256

#endif // CONFIG_H