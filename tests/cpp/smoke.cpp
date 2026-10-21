// Links the C++ core (mbedtls_py::core) without Python and hammers the parts the threading layer protects:
// concurrent hashing and the shared PSA random generator. Run under ThreadSanitizer in CI.
#include <mbedtls_py_threading.h>
#include <psa/crypto.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

static const unsigned char kAbc[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
                                       0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
                                       0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};

int main() {
    mbedtls_py_threading_init();
    if (psa_crypto_init() != PSA_SUCCESS) {
        std::fprintf(stderr, "psa_crypto_init failed\n");
        return 1;
    }

    std::atomic<int> failures{0};
    auto work = [&failures] {
        for (int i = 0; i < 200; ++i) {
            unsigned char out[32], rnd[32];
            size_t len = 0;
            if (psa_hash_compute(PSA_ALG_SHA_256, reinterpret_cast<const unsigned char*>("abc"), 3, out,
                                 sizeof out, &len) != PSA_SUCCESS ||
                len != 32 || std::memcmp(out, kAbc, 32) != 0 || psa_generate_random(rnd, sizeof rnd) != PSA_SUCCESS) {
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
