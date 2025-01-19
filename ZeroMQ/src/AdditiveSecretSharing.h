
#pragma once
#include <vector>
#include <random>
#include <cstdint>

/**
 * @brief Provides basic additive secret sharing functionality for integer secrets.
 */
class AdditiveSecretSharing {
public:
    /**
     * @brief Generates n additive shares of a secret integer.
     *        sum(shares) = secret (in normal integer arithmetic).
     * @param secret The original secret value.
     * @param numParties Number of shares/parties.
     * @return Vector of shares (size = numParties).
     */
    static std::vector<int> generateShares(int secret, int numParties) {
        std::vector<int> shares(numParties, 0);

        // Simple approach: random shares, final share = secret - sum(other shares)
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(-1000, 1000);

        int sumSoFar = 0;
        for(int i = 0; i < numParties - 1; ++i) {
            shares[i] = dist(rng);
            sumSoFar += shares[i];
        }
        shares[numParties - 1] = secret - sumSoFar; // ensure the sum is correct
        return shares;
    }

    /**
     * @brief Reconstructs the secret from all additive shares.
     * @param shares Vector of shares from each party.
     * @return The reconstructed secret value.
     */
    static int reconstructSecret(const std::vector<int>& shares) {
        int sum = 0;
        for (auto s : shares) {
            sum += s;
        }
        return sum;
    }

    // ...possible extensions for modular arithmetic or large values...
};