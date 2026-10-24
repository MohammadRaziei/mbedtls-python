#include <mbedtls.hpp>
#include <mbedtls.hpp>
#include <mbedtls/build_info.h>
#include <psa/crypto.h>

#include <atomic>
#include <thread>
#include <vector>

#include "common.hpp"
#include "utest.h"

using namespace mbedtls;

static const unsigned char kAbc[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
                                       0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
                                       0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
static const std::string kAbcDigest(reinterpret_cast<const char*>(kAbc), 32);

// The version of Mbed TLS itself, not of this project.
UTEST(version, matches_mbedtls) {
    ASSERT_TRUE(version_string() == MBEDTLS_VERSION_STRING);
    ASSERT_EQ(version_hex(), (uint32_t)MBEDTLS_VERSION_NUMBER);
    ASSERT_EQ(version_hex() >> 24, (uint32_t)MBEDTLS_VERSION_MAJOR);
}

UTEST(crypto, init_is_idempotent_and_psa_is_usable) {
    init();
    init();
    unsigned char rnd[16];
    ASSERT_EQ(psa_generate_random(rnd, sizeof rnd), PSA_SUCCESS);
}

UTEST(crypto, sha256_abc) {
    ASSERT_TRUE(digest("sha256", "abc") == kAbcDigest);
}

UTEST(crypto, digest_object_is_reusable) {
    const Digest d("sha256");
    ASSERT_EQ(d.size(), (size_t)32);
    ASSERT_TRUE(d.name() == "sha256");
    ASSERT_TRUE(d.compute("abc") == kAbcDigest);
    ASSERT_TRUE(d.compute("abc") == kAbcDigest);
}

// RFC 4231, test case 1.
UTEST(crypto, hmac_sha256_rfc4231) {
    static const unsigned char kHmac[32] = {0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf,
                                            0xce, 0xaf, 0x0b, 0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83,
                                            0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};
    const Hmac mac("sha256", std::string(20, '\x0b'));
    ASSERT_TRUE(mac.compute("Hi There") == std::string(reinterpret_cast<const char*>(kHmac), 32));
}

UTEST(crypto, hmac_empty_key_is_allowed) {
    ASSERT_EQ(hmac("sha256", "", "abc").size(), (size_t)32);
}

UTEST(crypto, unknown_algorithm_throws) {
    ASSERT_TRUE(throws<UnknownAlgorithm>([] { Digest("nope"); }));
    ASSERT_TRUE(throws<std::invalid_argument>([] { hmac("nope", "k", "d"); }));
    ASSERT_TRUE(throws<UnknownAlgorithm>([] { digest(std::string_view("sha256\0junk", 11), "abc"); }));
}

UTEST(crypto, algorithms_listed) {
    const auto names = hash_algorithms();
    ASSERT_EQ(names.size(), (size_t)10);
    for (const auto& n : names) ASSERT_EQ(Digest(n).compute("abc").size(), Digest(n).size());
}

UTEST(crypto, random_bytes_length_and_variety) {
    ASSERT_EQ(random_bytes(0).size(), (size_t)0);
    const std::string a = random_bytes(32), b = random_bytes(32);
    ASSERT_EQ(a.size(), (size_t)32);
    ASSERT_TRUE(a != b);
}

// What the threading layer protects: hashing, HMAC (volatile key slots) and the shared PSA random generator.
// Run under ThreadSanitizer in CI. (utest asserts are not thread-safe, so workers only count failures.)
UTEST(crypto, concurrent_digest_hmac_random) {
    std::atomic<int> failures{0};
    auto work = [&failures] {
        const Hmac mac("sha512", "");
        for (int i = 0; i < 200; ++i)
            if (digest("sha256", "abc") != kAbcDigest || mac.compute("x").size() != 64 || random_bytes(32).size() != 32)
                ++failures;
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back(work);
    for (auto& t : threads) t.join();
    ASSERT_EQ(failures.load(), 0);
}
