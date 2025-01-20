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

void AdditiveSecretSharing::generateShares(ShareType secret, int numParties, std::vector<ShareType>& sharesOut) {
    sharesOut.resize(numParties);

    BIGNUM* sumSoFar = newBigInt(); // = 0
    for(int i = 0; i < numParties; i++) {
        sharesOut[i] = newBigInt();
    }

    for(int i = 0; i < numParties - 1; i++) {
        BN_rand_range(sharesOut[i], getPrime()); 
        BN_mod_add(sumSoFar, sumSoFar, sharesOut[i], getPrime(), getCtx());
    }

    BIGNUM* tmp = newBigInt();
    BN_mod_sub(tmp, secret, sumSoFar, getPrime(), getCtx());
    BN_copy(sharesOut[numParties - 1], tmp);

    BN_free(tmp);
    BN_free(sumSoFar);
}

void AdditiveSecretSharing::reconstructSecret(const std::vector<ShareType>& shares, ShareType &result) {
    BIGNUM* total = newBigInt();
    for (auto s : shares) {
        BN_mod_add(total, total, s, getPrime(), getCtx());
    }
    // Copy final value into caller-provided space
    BN_copy(result, total);
    BN_free(total);
}

// Remove or comment out any leftover function:
// ShareType AdditiveSecretSharing::addShares(ShareType x, ShareType y) { ... }

// Keep the void-based addShares below:
void AdditiveSecretSharing::addShares(ShareType x, ShareType y, ShareType &result) {
    BN_mod_add(result, x, y, getPrime(), getCtx());
}

void AdditiveSecretSharing::addShares(const std::vector<ShareType>& inputShares, ShareType &result) {
    BN_zero(result);
    for (auto s : inputShares) {
        BN_mod_add(result, result, s, getPrime(), getCtx());
    }
}

// Remove or comment out any leftover function:
// ShareType AdditiveSecretSharing::multiplyShares(ShareType x, ShareType y, const BeaverTriple &triple) { ... }

// Keep the void-based multiplyShares below:
void AdditiveSecretSharing::multiplyShares(ShareType x, ShareType y,
                                           const BeaverTriple &triple, ShareType &product)
{
    // ...existing code...
    BIGNUM* d = newBigInt();
    BIGNUM* e = newBigInt();
    BN_mod_sub(d, x, triple.a, getPrime(), getCtx());
    BN_mod_sub(e, y, triple.b, getPrime(), getCtx());

    BN_copy(product, triple.c);
    BIGNUM* tmp = newBigInt();

    // ...existing code...
    BN_mod_mul(tmp, triple.a, e, getPrime(), getCtx());
    BN_mod_add(product, product, tmp, getPrime(), getCtx());

    // ...existing code...
    BN_free(tmp);
    BN_free(d);
    BN_free(e);
}