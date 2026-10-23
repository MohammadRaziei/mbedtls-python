#ifndef MBEDTLS_CPP_X509_H
#define MBEDTLS_CPP_X509_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MBEDTLS_CPP_X509_NAME_MAX 1024
#define MBEDTLS_CPP_X509_SERIAL_MAX 64 /* RFC 5280 caps serial numbers at 20 bytes */

/* mbedtls_cpp_x509_verify(): the chain did not verify, `flags` and `info` say why. Any other non-zero return is a
 * (negative) Mbed TLS error code. */
#define MBEDTLS_CPP_X509_VERIFY_FAILED 1

/* A parsed certificate chain. Opaque, owned by the caller, released with mbedtls_cpp_x509_free(). */
typedef struct mbedtls_cpp_x509 mbedtls_cpp_x509;

typedef struct {
    const unsigned char* der; /* points into the parsed chain: valid until mbedtls_cpp_x509_free() */
    size_t der_len;
    int version; /* 1, 2 or 3 */
    int is_ca;   /* basicConstraints cA */
    char subject[MBEDTLS_CPP_X509_NAME_MAX]; /* NUL-terminated, Mbed TLS's "CN=..., O=..." formatting */
    char issuer[MBEDTLS_CPP_X509_NAME_MAX];
    unsigned char serial[MBEDTLS_CPP_X509_SERIAL_MAX]; /* big endian */
    size_t serial_len;
    int not_before[6]; /* year, month, day, hour, minute, second (UTC) */
    int not_after[6];
} mbedtls_cpp_x509_info;

/* Parses `pem` (text holding one or more "BEGIN CERTIFICATE" blocks) or one binary DER certificate. Returns 0 and
 * a chain in *out; a negative Mbed TLS error code; or a positive number of certificates in a PEM bundle that could
 * not be parsed (nothing is returned then). */
int mbedtls_cpp_x509_parse(const unsigned char* buf, size_t len, int pem, mbedtls_cpp_x509** out);
void mbedtls_cpp_x509_free(mbedtls_cpp_x509* chain);
size_t mbedtls_cpp_x509_count(const mbedtls_cpp_x509* chain);
/* Fills `out` for the i-th certificate of the chain. */
int mbedtls_cpp_x509_get_info(const mbedtls_cpp_x509* chain, size_t i, mbedtls_cpp_x509_info* out);

/* Verifies DER certificate chain[0] (chain[1..] are intermediates, in any order) against the DER trust anchors. The
 * hostname, when not NULL, must match a subjectAltName (or the common name when there is none). `info` receives
 * Mbed TLS's description of the problems, one per line. Uses Mbed TLS's default profile (SHA-2, RSA >= 2048 bits,
 * the common curves). Returns 0, MBEDTLS_CPP_X509_VERIFY_FAILED, or a negative Mbed TLS error code. */
int mbedtls_cpp_x509_verify(const unsigned char* const* chain, const size_t* chain_len, size_t chain_n,
                           const unsigned char* const* trust, const size_t* trust_len, size_t trust_n,
                           const char* hostname, uint32_t* flags, char* info, size_t info_size);

/* Text for an error code returned above. */
void mbedtls_cpp_x509_strerror(int code, char* buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif
