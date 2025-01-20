#include "AdditiveSecretSharing.h"
#include "config.h"
#include <openssl/bn.h>
#include <chrono>
#include <stdexcept>

// Thread-local context: used for BN operations
static thread_local BN_CTX* s_bnCtx = nullptr;
// Thread-local prime
static thread_local BIGNUM* s_prime = nullptr;

BIGNUM* AdditiveSecretSharing::getPrime() {
    if (!s_prime) {
        s_prime = BN_new();
        if (!s_prime) throw std::runtime_error("Failed to allocate prime BIGNUM");
        BN_dec2bn(&s_prime, PRIME_128_STR); // from config.h
    }
    return s_prime;
}

BN_CTX* AdditiveSecretSharing::getCtx() {
    if (!s_bnCtx) {
        s_bnCtx = BN_CTX_new();
        if (!s_bnCtx) throw std::runtime_error("Failed to create BN_CTX");
    }
    return s_bnCtx;
}

ShareType AdditiveSecretSharing::newBigInt() {
    BIGNUM* bn = BN_new();
    if (!bn) throw std::runtime_error("Failed to create BIGNUM");
    BN_zero(bn);
    return bn;
}

ShareType AdditiveSecretSharing::cloneBigInt(ShareType source) {
    if (!source) return nullptr;
    BIGNUM* bn = BN_new();
    if (!bn) throw std::runtime_error("Failed to clone BIGNUM");
    BN_copy(bn, source);
    return bn;
}

std::vector<ShareType> AdditiveSecretSharing::generateShares(ShareType secret, int numParties) {
    std::vector<ShareType> shares(numParties);
    for (auto &s : shares) {
        s = newBigInt();
    }

    // sumSoFar = 0
    BIGNUM* sumSoFar = newBigInt();

    // For numParties-1 shares, pick random in [0..prime-1]
    for(int i = 0; i < numParties - 1; i++) {
        BN_rand_range(shares[i], getPrime()); // shares[i] in [0..prime-1]
        // sumSoFar = sumSoFar + shares[i] mod prime
        BN_mod_add(sumSoFar, sumSoFar, shares[i], getPrime(), getCtx());
    }

    // final share = (secret - sumSoFar) mod prime
    BIGNUM* tmp = newBigInt(); // for intermediate
    BN_mod_sub(tmp, secret, sumSoFar, getPrime(), getCtx());
    BN_copy(shares[numParties - 1], tmp);

    BN_free(tmp);
    BN_free(sumSoFar);
    return shares;
}

ShareType AdditiveSecretSharing::reconstructSecret(const std::vector<ShareType>& shares) {
    // total = 0
    BIGNUM* total = newBigInt();
    for (auto s : shares) {
        // total = total + s mod prime
        BN_mod_add(total, total, s, getPrime(), getCtx());
    }
    return total; // caller must BN_free() when done
}

ShareType AdditiveSecretSharing::addShares(ShareType x, ShareType y) {
    BIGNUM* result = newBigInt();
    BN_mod_add(result, x, y, getPrime(), getCtx());
    return result; // caller frees
}

ShareType AdditiveSecretSharing::multiplyShares(ShareType x, ShareType y, const BeaverTriple &triple) {
    // This is just a placeholder to illustrate how it might work with Beaver.
    // Real usage would need each party to hold partial quadruple-shares plus communication.

    // Create BNs for d = (x-a), e = (y-b)
    BIGNUM* d = newBigInt();
    BIGNUM* e = newBigInt();
    BN_mod_sub(d, x, triple.a, getPrime(), getCtx());
    BN_mod_sub(e, y, triple.b, getPrime(), getCtx());

    // part = c + a*e + b*d + d*e
    BIGNUM* part = cloneBigInt(triple.c);
    BIGNUM* tmp = newBigInt();

    // a*e
    BN_mod_mul(tmp, triple.a, e, getPrime(), getCtx());
    BN_mod_add(part, part, tmp, getPrime(), getCtx());

    // b*d
    BN_mod_mul(tmp, triple.b, d, getPrime(), getCtx());
    BN_mod_add(part, part, tmp, getPrime(), getCtx());

    // d*e
    BN_mod_mul(tmp, d, e, getPrime(), getCtx());
    BN_mod_add(part, part, tmp, getPrime(), getCtx());

    BN_free(tmp);
    BN_free(d);
    BN_free(e);

    return part; // caller frees
}