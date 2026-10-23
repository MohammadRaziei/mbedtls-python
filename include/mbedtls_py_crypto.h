#ifndef MBEDTLS_PY_CRYPTO_H
#define MBEDTLS_PY_CRYPTO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Largest digest of the supported algorithms (SHA-512, SHA3-512). */
#define MBEDTLS_PY_MAX_DIGEST_SIZE 64

/* Return value for an algorithm name that is not in the table. Every other non-zero return is a (negative)
 * psa_status_t passed through unchanged. */
#define MBEDTLS_PY_ERR_UNKNOWN_ALGORITHM 1

/* Name of the i-th supported hash ("sha256", "sha3_512", ...; hashlib's spelling), or NULL past the end. */
const char* mbedtls_py_hash_name(size_t i);

/* One-shot digest. `out` must hold MBEDTLS_PY_MAX_DIGEST_SIZE bytes. Needs psa_crypto_init() to have run. */
int mbedtls_py_digest(const char* name, const unsigned char* data, size_t data_len, unsigned char* out,
                      size_t* out_len);

/* One-shot HMAC, same contract. `key` may be empty. */
int mbedtls_py_hmac(const char* name, const unsigned char* key, size_t key_len, const unsigned char* data,
                    size_t data_len, unsigned char* out, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif
