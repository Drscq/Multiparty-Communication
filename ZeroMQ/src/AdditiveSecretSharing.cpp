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

    BIGNUM* sumSoFar = newBigInt(); // Initialize to 0
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
    // 1) Locally compute d = (x - a), e = (y - b)
    BIGNUM* d = newBigInt();
    BIGNUM* e = newBigInt();
    if(!BN_mod_sub(d, x, triple.a, getPrime(), getCtx())) {
        throw std::runtime_error("BN_mod_sub failed for d");
    }
    if(!BN_mod_sub(e, y, triple.b, getPrime(), getCtx())) {
        BN_free(d);
        throw std::runtime_error("BN_mod_sub failed for e");
    }

    // 2) Compute a * E and b * D
    BIGNUM* aE = newBigInt();
    BIGNUM* bD = newBigInt();
    if(!BN_mod_mul(aE, triple.a, e, getPrime(), getCtx())) {
        BN_free(d);
        BN_free(e);
        BN_free(aE);
        throw std::runtime_error("BN_mod_mul failed for aE");
    }
    if(!BN_mod_mul(bD, triple.b, d, getPrime(), getCtx())) {
        BN_free(d);
        BN_free(e);
        BN_free(aE);
        BN_free(bD);
        throw std::runtime_error("BN_mod_mul failed for bD");
    }

    // 3) Compute D * E
    BIGNUM* DE = newBigInt();
    if(!BN_mod_mul(DE, d, e, getPrime(), getCtx())) {
        BN_free(d);
        BN_free(e);
        BN_free(aE);
        BN_free(bD);
        BN_free(DE);
        throw std::runtime_error("BN_mod_mul failed for DE");
    }

    // 4) Compute c + a * E + b * D + D * E
    if(!BN_mod_add(product, triple.c, aE, getPrime(), getCtx())) {
        throw std::runtime_error("BN_mod_add failed for c + aE");
    }
    if(!BN_mod_add(product, product, bD, getPrime(), getCtx())) {
        throw std::runtime_error("BN_mod_add failed for + bD");
    }
    if(!BN_mod_add(product, product, DE, getPrime(), getCtx())) {
        throw std::runtime_error("BN_mod_add failed for + DE");
    }

    // 5) Free temporary BIGNUMs
    BN_free(d);
    BN_free(e);
    BN_free(aE);
    BN_free(bD);
    BN_free(DE);
}