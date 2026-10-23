#ifndef MBEDTLS_CPP_THREADING_H
#define MBEDTLS_CPP_THREADING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Call before any other Mbed TLS / PSA function. Safe to call more than once and from several threads.
 * Registers the alt threading layer when the build selected it (Windows, or MBEDTLS_CPP_THREADING_ALT); on POSIX
 * the pthread layer needs nothing and this does nothing. */
void mbedtls_cpp_threading_init(void);

#ifdef __cplusplus
}
#endif

#endif
