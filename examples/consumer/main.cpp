// Smallest possible user of the installed package: SHA-256("abc") through the PSA API.
#include <mbedtls_cpp_crypto.h>
#include <mbedtls_cpp_threading.h>
#include <psa/crypto.h>

#include <cstdio>
#include <cstring>

int main() {
    mbedtls_cpp_threading_init();  // before any other Mbed TLS call
    if (psa_crypto_init() != PSA_SUCCESS) return 1;

    unsigned char out[MBEDTLS_CPP_MAX_DIGEST_SIZE];
    size_t len = 0;
    if (mbedtls_cpp_digest("sha256", reinterpret_cast<const unsigned char*>("abc"), 3, out, &len) != 0) return 1;
    for (size_t i = 0; i < len; ++i) std::printf("%02x", out[i]);
    std::printf("\n");
    return len == 32 && out[0] == 0xba && out[31] == 0xad ? 0 : 1;
}
