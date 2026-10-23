#include "mbedtls_py_crypto.h"

#include <psa/crypto.h>

#include <cstring>

namespace {

struct Hash {
    const char* name;
    psa_algorithm_t alg;
};

constexpr Hash kHashes[] = {
    {"md5", PSA_ALG_MD5},           {"sha1", PSA_ALG_SHA_1},         {"sha224", PSA_ALG_SHA_224},
    {"sha256", PSA_ALG_SHA_256},    {"sha384", PSA_ALG_SHA_384},     {"sha512", PSA_ALG_SHA_512},
    {"sha3_224", PSA_ALG_SHA3_224}, {"sha3_256", PSA_ALG_SHA3_256},  {"sha3_384", PSA_ALG_SHA3_384},
    {"sha3_512", PSA_ALG_SHA3_512},
};

const Hash* find(const char* name) {
    for (const Hash& h : kHashes)
        if (std::strcmp(h.name, name) == 0) return &h;
    return nullptr;
}

} // namespace

extern "C" const char* mbedtls_py_hash_name(size_t i) {
    return i < sizeof kHashes / sizeof kHashes[0] ? kHashes[i].name : nullptr;
}

extern "C" int mbedtls_py_digest(const char* name, const unsigned char* data, size_t data_len, unsigned char* out,
                                 size_t* out_len) {
    const Hash* h = find(name);
    if (!h) return MBEDTLS_PY_ERR_UNKNOWN_ALGORITHM;
    return psa_hash_compute(h->alg, data, data_len, out, MBEDTLS_PY_MAX_DIGEST_SIZE, out_len);
}

extern "C" int mbedtls_py_hmac(const char* name, const unsigned char* key, size_t key_len,
                               const unsigned char* data, size_t data_len, unsigned char* out, size_t* out_len) {
    const Hash* h = find(name);
    if (!h) return MBEDTLS_PY_ERR_UNKNOWN_ALGORITHM;

    // PSA refuses to import an empty key. HMAC zero-pads the key to the block size, so the empty key and a single
    // zero byte give the same result.
    static const unsigned char zero = 0;
    if (key_len == 0) {
        key = &zero;
        key_len = 1;
    }

    // ponytail: the key is imported (and destroyed) on every call, a volatile PSA key costs a few microseconds.
    // Upgrade path: an Hmac object that holds the key id, once an incremental API exists (it needs its own lock
    // to be shared between threads).
    const psa_algorithm_t alg = PSA_ALG_HMAC(h->alg);
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, alg);

    psa_key_id_t id = 0;
    psa_status_t status = psa_import_key(&attr, key, key_len, &id);
    if (status != PSA_SUCCESS) return status;
    status = psa_mac_compute(id, alg, data, data_len, out, MBEDTLS_PY_MAX_DIGEST_SIZE, out_len);
    psa_destroy_key(id);
    return status;
}
