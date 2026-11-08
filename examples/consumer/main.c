/* Smallest possible user of the installed Mbed TLS (find_package(MbedTLS)): SHA-256("abc") through PSA, the C API itself. */
#include <mbedtls/build_info.h>
#include <psa/crypto.h>

#include <stdio.h>

#if defined(MBEDTLS_THREADING_ALT)
/* The build uses the alt threading layer (Windows always does: it has no pthread layer). The application registers it
 * once, before any other Mbed TLS call. (The Python package does it for itself.) */
#include <mbedtls/threading.h>
#if defined(_WIN32)
static int mutex_init(mbedtls_platform_mutex_t* m) { InitializeSRWLock(m); return 0; }
static void mutex_destroy(mbedtls_platform_mutex_t* m) { (void)m; }
static int mutex_lock(mbedtls_platform_mutex_t* m) { AcquireSRWLockExclusive(m); return 0; }
static int mutex_unlock(mbedtls_platform_mutex_t* m) { ReleaseSRWLockExclusive(m); return 0; }
static int cond_init(mbedtls_platform_condition_variable_t* c) { InitializeConditionVariable(c); return 0; }
static void cond_destroy(mbedtls_platform_condition_variable_t* c) { (void)c; }
static int cond_signal(mbedtls_platform_condition_variable_t* c) { WakeConditionVariable(c); return 0; }
static int cond_broadcast(mbedtls_platform_condition_variable_t* c) { WakeAllConditionVariable(c); return 0; }
static int cond_wait(mbedtls_platform_condition_variable_t* c, mbedtls_platform_mutex_t* m) {
    return SleepConditionVariableSRW(c, m, INFINITE, 0) ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR;
}
#else
static int mutex_init(mbedtls_platform_mutex_t* m) { return pthread_mutex_init(m, NULL) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
static void mutex_destroy(mbedtls_platform_mutex_t* m) { pthread_mutex_destroy(m); }
static int mutex_lock(mbedtls_platform_mutex_t* m) { return pthread_mutex_lock(m) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
static int mutex_unlock(mbedtls_platform_mutex_t* m) { return pthread_mutex_unlock(m) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
static int cond_init(mbedtls_platform_condition_variable_t* c) { return pthread_cond_init(c, NULL) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
static void cond_destroy(mbedtls_platform_condition_variable_t* c) { pthread_cond_destroy(c); }
static int cond_signal(mbedtls_platform_condition_variable_t* c) { return pthread_cond_signal(c) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
static int cond_broadcast(mbedtls_platform_condition_variable_t* c) { return pthread_cond_broadcast(c) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
static int cond_wait(mbedtls_platform_condition_variable_t* c, mbedtls_platform_mutex_t* m) {
    return pthread_cond_wait(c, m) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR;
}
#endif
#endif

int main(void) {
#if defined(MBEDTLS_THREADING_ALT)
    mbedtls_threading_set_alt(mutex_init, mutex_destroy, mutex_lock, mutex_unlock, cond_init, cond_destroy, cond_signal,
                              cond_broadcast, cond_wait);
#endif
    if (psa_crypto_init() != PSA_SUCCESS) return 1;

    unsigned char sum[PSA_HASH_MAX_SIZE];
    size_t len = 0;
    if (psa_hash_compute(PSA_ALG_SHA_256, (const unsigned char*)"abc", 3, sum, sizeof sum, &len) != PSA_SUCCESS) return 1;
    for (size_t i = 0; i < len; ++i) printf("%02x", sum[i]);
    printf("\n");
    return len == 32 && sum[0] == 0xba ? 0 : 1;
}
