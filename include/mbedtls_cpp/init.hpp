#pragma once

namespace mbedtls_cpp {

// Registers the alt threading layer when the build selected it (Windows, or MBEDTLS_CPP_THREADING_ALT; on POSIX the
// pthread layer needs nothing) and runs psa_crypto_init(). Safe to call more than once and from several threads. The
// classes and functions of this library call it themselves on first use; call it explicitly before using the Mbed TLS
// / PSA C API directly. Throws Error when psa_crypto_init() fails.
void init();

}  // namespace mbedtls_cpp
