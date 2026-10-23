#include <mbedtls_cpp_crypto.h>
#include <psa/crypto.h>

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include "common.hpp"
#include "utest.h"

static const unsigned char kAbc[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
                                       0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
                                       0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};

UTEST(crypto, psa_is_initialized) {
    unsigned char rnd[16];
    ASSERT_EQ(psa_generate_random(rnd, sizeof rnd), PSA_SUCCESS);
}

UTEST(crypto, sha256_abc) {
    unsigned char out[MBEDTLS_CPP_MAX_DIGEST_SIZE];
    size_t len = 0;
    ASSERT_EQ(mbedtls_cpp_digest("sha256", reinterpret_cast<const unsigned char*>("abc"), 3, out, &len), 0);
    ASSERT_EQ(len, (size_t)32);
    ASSERT_EQ(std::memcmp(out, kAbc, 32), 0);
}

// RFC 4231, test case 1.
UTEST(crypto, hmac_sha256_rfc4231) {
    static const unsigned char kHmac[32] = {0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf,
                                            0xce, 0xaf, 0x0b, 0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83,
                                            0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};
    unsigned char key[20], out[MBEDTLS_CPP_MAX_DIGEST_SIZE];
    std::memset(key, 0x0b, sizeof key);
    size_t len = 0;
    ASSERT_EQ(mbedtls_cpp_hmac("sha256", key, sizeof key, reinterpret_cast<const unsigned char*>("Hi There"), 8, out, &len), 0);
    ASSERT_EQ(len, (size_t)32);
    ASSERT_EQ(std::memcmp(out, kHmac, 32), 0);
}

UTEST(crypto, unknown_algorithm_is_rejected) {
    unsigned char key[1] = {0}, out[MBEDTLS_CPP_MAX_DIGEST_SIZE];
    size_t len = 0;
    ASSERT_EQ(mbedtls_cpp_digest("nope", key, 1, out, &len), MBEDTLS_CPP_ERR_UNKNOWN_ALGORITHM);
}

// What the threading layer protects: hashing, HMAC (volatile key slots) and the shared PSA random generator.
// Run under ThreadSanitizer in CI. (utest asserts are not thread-safe, so workers only count failures.)
UTEST(crypto, concurrent_digest_hmac_random) {
    std::atomic<int> failures{0};
    auto work = [&failures] {
        for (int i = 0; i < 200; ++i) {
            unsigned char out[MBEDTLS_CPP_MAX_DIGEST_SIZE], mac[MBEDTLS_CPP_MAX_DIGEST_SIZE], rnd[32];
            size_t len = 0, mac_len = 0;
            if (mbedtls_cpp_digest("sha256", reinterpret_cast<const unsigned char*>("abc"), 3, out, &len) != 0 ||
                len != 32 || std::memcmp(out, kAbc, 32) != 0 ||
                mbedtls_cpp_hmac("sha512", rnd, 0, out, 32, mac, &mac_len) != 0 || mac_len != 64 ||
                psa_generate_random(rnd, sizeof rnd) != PSA_SUCCESS)
                ++failures;
        }
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back(work);
    for (auto& t : threads) t.join();
    ASSERT_EQ(failures.load(), 0);
}
