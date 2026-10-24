#include "mbedtls_cpp/crypto.hpp"

#include <mbedtls/platform_util.h>
#include <psa/crypto.h>

#include <iterator>

#include "mbedtls_cpp/init.hpp"

namespace mbedtls_cpp {
namespace {

struct Hash {
    std::string_view name;
    psa_algorithm_t alg;
};

constexpr Hash kHashes[] = {
    {"md5", PSA_ALG_MD5},           {"sha1", PSA_ALG_SHA_1},         {"sha224", PSA_ALG_SHA_224},
    {"sha256", PSA_ALG_SHA_256},    {"sha384", PSA_ALG_SHA_384},     {"sha512", PSA_ALG_SHA_512},
    {"sha3_224", PSA_ALG_SHA3_224}, {"sha3_256", PSA_ALG_SHA3_256},  {"sha3_384", PSA_ALG_SHA3_384},
    {"sha3_512", PSA_ALG_SHA3_512},
};

constexpr size_t kMaxDigestSize = 64;  // SHA-512, SHA3-512

psa_algorithm_t find(std::string_view name) {
    for (const Hash& h : kHashes)
        if (h.name == name) return h.alg;
    throw UnknownAlgorithm(name);
}

[[noreturn]] void fail(const char* what, psa_status_t status) {
    throw Error(std::string(what) + " failed with PSA status " + std::to_string(status), status);
}

const unsigned char* bytes(std::string_view s) { return reinterpret_cast<const unsigned char*>(s.data()); }

}  // namespace

std::vector<std::string> hash_algorithms() {
    std::vector<std::string> names;
    for (const Hash& h : kHashes) names.emplace_back(h.name);
    return names;
}

Digest::Digest(std::string_view name) : name_(name), alg_(find(name)) {}

size_t Digest::size() const noexcept { return PSA_HASH_LENGTH(alg_); }

std::string Digest::compute(std::string_view data) const {
    init();
    unsigned char out[kMaxDigestSize];
    size_t len = 0;
    const psa_status_t status = psa_hash_compute(alg_, bytes(data), data.size(), out, sizeof out, &len);
    if (status != PSA_SUCCESS) fail("digest", status);
    return std::string(reinterpret_cast<const char*>(out), len);
}

Hmac::Hmac(std::string_view name, std::string_view key) : digest_(name), key_(key) {}

Hmac::~Hmac() { mbedtls_platform_zeroize(key_.data(), key_.size()); }

std::string Hmac::compute(std::string_view data) const {
    init();
    // PSA refuses to import an empty key. HMAC zero-pads the key to the block size, so the empty key and a single
    // zero byte give the same result.
    static const unsigned char zero = 0;
    const unsigned char* key = key_.empty() ? &zero : bytes(key_);
    const size_t key_len = key_.empty() ? 1 : key_.size();

    // ponytail: the key is imported (and destroyed) on every call, a volatile PSA key costs a few microseconds and
    // keeps concurrent calls independent. Upgrade path: hold the key id in this object, once an incremental API exists
    // (it needs its own lock to be shared between threads).
    const psa_algorithm_t alg = PSA_ALG_HMAC(find(digest_.name()));
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, alg);

    psa_key_id_t id = 0;
    psa_status_t status = psa_import_key(&attr, key, key_len, &id);
    if (status != PSA_SUCCESS) fail("hmac", status);
    unsigned char out[kMaxDigestSize];
    size_t len = 0;
    status = psa_mac_compute(id, alg, bytes(data), data.size(), out, sizeof out, &len);
    psa_destroy_key(id);
    if (status != PSA_SUCCESS) fail("hmac", status);
    return std::string(reinterpret_cast<const char*>(out), len);
}

std::string digest(std::string_view name, std::string_view data) { return Digest(name).compute(data); }

std::string hmac(std::string_view name, std::string_view key, std::string_view data) {
    return Hmac(name, key).compute(data);
}

std::string random_bytes(size_t n) {
    init();
    std::string out(n, '\0');
    if (n == 0) return out;
    const psa_status_t status = psa_generate_random(reinterpret_cast<unsigned char*>(out.data()), n);
    if (status != PSA_SUCCESS) fail("random_bytes", status);
    return out;
}

}  // namespace mbedtls_cpp
