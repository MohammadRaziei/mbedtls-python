/* Appended to TF-PSA-Crypto's default configuration (TF_PSA_CRYPTO_USER_CONFIG_FILE, set in the root CMakeLists.txt).
 * The submodule itself is never edited.
 *
 * Thread safety. TF-PSA-Crypto keeps process-wide state (the PSA random generator, the key slots) that is only
 * locked when the threading layer is on. Without it two threads can draw from the DRBG at the same time and get the
 * same "random" bytes. Python threads (above all on free-threaded builds) make that a real risk, so it is always on.
 *
 *  - POSIX: the pthread layer, nothing to register.
 *  - Windows has no pthread layer: the alt layer (src/threading.cpp) is registered once, over SRWLOCK and
 *    CONDITION_VARIABLE. MBEDTLS_PY_THREADING_ALT selects the alt layer on POSIX too, so the same registration
 *    code can be run under ThreadSanitizer in CI.
 */
#define MBEDTLS_THREADING_C
#if defined(_WIN32) || defined(MBEDTLS_PY_THREADING_ALT)
#define MBEDTLS_THREADING_ALT
#else
#define MBEDTLS_THREADING_PTHREAD
#endif

/* The self tests are not used, and while they are compiled in, ecp.c counts its operations in process-wide counters
 * (add_count, dbl_count, mul_count) that every thread increments with no lock: a harmless race by design that makes
 * ThreadSanitizer useless for the rest. Off, that code is not built at all. */
#undef MBEDTLS_SELF_TEST
