// Links the C++ core (mbedtls_cpp::core) without Python and hammers the parts the threading layer protects:
// concurrent hashing, HMAC (volatile key slots) and the shared PSA random generator. Run under ThreadSanitizer in CI.
#include <mbedtls_cpp_crypto.h>
#include <mbedtls_cpp_threading.h>
#include <mbedtls_cpp_x509.h>
#include <psa/crypto.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

static const unsigned char kAbc[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
                                       0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
                                       0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};

static std::string read_file(const char* name) {
    std::ifstream f(std::string(MBEDTLS_CPP_FIXTURES_DIR) + "/" + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

// Parse a PEM certificate and hand back its DER (empty on failure).
static std::string der_of(const char* name) {
    const std::string pem = read_file(name);
    mbedtls_cpp_x509* chain = nullptr;
    std::string der;
    if (mbedtls_cpp_x509_parse(reinterpret_cast<const unsigned char*>(pem.data()), pem.size(), 1, &chain) == 0) {
        mbedtls_cpp_x509_info info;
        if (mbedtls_cpp_x509_get_info(chain, 0, &info) == 0)
            der.assign(reinterpret_cast<const char*>(info.der), info.der_len);
    }
    mbedtls_cpp_x509_free(chain);
    return der;
}

int main() {
    mbedtls_cpp_threading_init();
    if (psa_crypto_init() != PSA_SUCCESS) {
        std::fprintf(stderr, "psa_crypto_init failed\n");
        return 1;
    }

    // HMAC-SHA256, RFC 4231 test case 1, and an unknown algorithm name.
    {
        static const unsigned char kHmac[32] = {0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf,
                                                0xce, 0xaf, 0x0b, 0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83,
                                                0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};
        unsigned char key[20], out[MBEDTLS_CPP_MAX_DIGEST_SIZE];
        std::memset(key, 0x0b, sizeof key);
        size_t len = 0;
        if (mbedtls_cpp_hmac("sha256", key, sizeof key, reinterpret_cast<const unsigned char*>("Hi There"), 8, out,
                            &len) != 0 ||
            len != 32 || std::memcmp(out, kHmac, 32) != 0) {
            std::fprintf(stderr, "hmac mismatch\n");
            return 1;
        }
        if (mbedtls_cpp_digest("nope", key, 1, out, &len) != MBEDTLS_CPP_ERR_UNKNOWN_ALGORITHM) {
            std::fprintf(stderr, "unknown algorithm not rejected\n");
            return 1;
        }
    }

    // X.509: parse the fixtures, then verify leaf -> int -> root from several threads (see tests/data).
    const std::string leaf = der_of("leaf.pem"), inter = der_of("int.pem"), root = der_of("root.pem");
    if (leaf.empty() || inter.empty() || root.empty()) {
        std::fprintf(stderr, "cannot parse the fixtures\n");
        return 1;
    }
    auto p = [](const std::string& d) { return reinterpret_cast<const unsigned char*>(d.data()); };
    const unsigned char* const chain[2] = {p(leaf), p(inter)};
    const size_t chain_len[2] = {leaf.size(), inter.size()};
    const unsigned char* const anchor[1] = {p(root)};
    const size_t anchor_len[1] = {root.size()};

    std::atomic<int> failures{0};
    auto verify = [&] {
        uint32_t flags = 0;
        char info[512];
        if (mbedtls_cpp_x509_verify(chain, chain_len, 2, anchor, anchor_len, 1, "leaf.example.test", &flags, info,
                                   sizeof info) != 0)
            ++failures;
        // Without the intermediate it must be refused, and say why.
        if (mbedtls_cpp_x509_verify(chain, chain_len, 1, anchor, anchor_len, 1, nullptr, &flags, info, sizeof info) !=
                MBEDTLS_CPP_X509_VERIFY_FAILED ||
            flags == 0 || info[0] == '\0')
            ++failures;
    };
    auto work = [&failures, &verify] {
        for (int i = 0; i < 5; ++i) verify();
        for (int i = 0; i < 200; ++i) {
            unsigned char out[MBEDTLS_CPP_MAX_DIGEST_SIZE], mac[MBEDTLS_CPP_MAX_DIGEST_SIZE], rnd[32];
            size_t len = 0, mac_len = 0;
            if (mbedtls_cpp_digest("sha256", reinterpret_cast<const unsigned char*>("abc"), 3, out, &len) != 0 ||
                len != 32 || std::memcmp(out, kAbc, 32) != 0 ||
                mbedtls_cpp_hmac("sha512", rnd, 0, out, 32, mac, &mac_len) != 0 || mac_len != 64 ||
                psa_generate_random(rnd, sizeof rnd) != PSA_SUCCESS) {
                ++failures;
            }
        }
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back(work);
    for (auto& t : threads) t.join();

    if (failures != 0) {
        std::fprintf(stderr, "%d failures\n", failures.load());
        return 1;
    }
    std::puts("ok");
    return 0;
}
