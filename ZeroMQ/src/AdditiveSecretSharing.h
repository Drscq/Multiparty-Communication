#pragma once
#include <vector>
#include <random>
#include <cstdint>
#include "config.h"
#include <openssl/bn.h>



/**
 * @brief Struct or class to represent a Beaver Triple (a, b, c).
 *        For brevity, not all details are shown.
 */
struct BeaverTriple {
    ShareType a;
    ShareType b;
    ShareType c;
};

/**
 * @brief Provides additive secret sharing functionality over a finite field.
 */
class AdditiveSecretSharing {
public:
    /**
     * @brief Generates n additive shares of 'secret', mod PRIME_MODULUS.
     * @param secret The original secret value in [0..PRIME_MODULUS-1].
     * @param numParties Number of shares/parties.
     * @param sharesOut Vector to store the generated shares (size = numParties).
     */
    static void generateShares(ShareType secret, int numParties, std::vector<ShareType>& sharesOut);
    /**
     * @brief Generates n additive shares of multiplication of secret with a mac key, mod PRIME_MODULUS.
     * @param secret The original secret value in [0..PRIME_MODULUS-1].
     * @param macKey The global MAC key for this party.
     * @param numParties Number of shares/parties.
     * @param sharesOut Vector to store the generated shares (size = numParties).
     */
    static void generateMacShares(ShareType secret, ShareType macKey, PARTY_ID_T numParties, std::vector<ShareType>& sharesOut);

    /**
     * @brief Reconstructs the secret from shares, mod PRIME_MODULUS.
     * @param shares Vector of shares from each party.
     * @param result The reconstructed secret value (mod PRIME_MODULUS).
     */
    static void reconstructSecret(const std::vector<ShareType>& shares, ShareType &result);

    /**
     * @brief Adds two share values (mod PRIME_MODULUS).
     * @param x A share value.
     * @param y A share value.
     * @param result The result of (x + y) mod PRIME_MODULUS.
     */
    static void addShares(ShareType x, ShareType y, ShareType &result);

    /**
     * @brief Adds multiple share values (mod PRIME_MODULUS).
     * @param inputShares Vector of share values.
     * @param result The result of summing all inputShares mod PRIME_MODULUS.
     */
    static void addShares(const std::vector<ShareType>& inputShares, ShareType &result);

    /**
     * @brief Multiplies two share values using Beaver Triples.
     * @param x A share value for secret X
     * @param y A share value for secret Y
     * @param triple The Beaver triple (a, b, c)
     * @param product The result of (x * y) mod PRIME_MODULUS
     */
    static void multiplyShares(ShareType x, ShareType y,
                               const BeaverTriple &triple, ShareType &product);

    /**
     * @brief Creates and returns a new BIGNUM with value = 0.
     */
    static ShareType newBigInt();

    /**
     * @brief Clones a BIGNUM into a new object.
     */
    static ShareType cloneBigInt(ShareType source);

    /**
     * @brief Returns a global prime BIGNUM* for modulo ops.
     */
    static ShareType getPrime();

    /**
     * @brief Thread-local RNG for random BN generation.
     */
    static BN_CTX* getCtx();

private:
    /**
     * @brief For demonstration, a naive random distribution used in generateShares().
     */
    static std::mt19937& getRng();
};