// ponytail: utest has no global setup hook, so a static initializer runs the two calls every Mbed TLS user must make
// first. Upgrade path: a custom main() once more than this is needed.
#include <mbedtls_cpp_threading.h>
#include <psa/crypto.h>

namespace {
struct Init {
    Init() {
        mbedtls_cpp_threading_init();
        psa_crypto_init();
    }
} init;
}  // namespace
