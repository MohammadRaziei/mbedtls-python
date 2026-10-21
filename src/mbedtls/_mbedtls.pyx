# cython: language_level=3, freethreading_compatible=True
# Stable-ABI (abi3, cp39) safe: no typed memoryviews (Py_buffer is only in the limited API from 3.11), no
# PyObject internals. Input is read in place when it is `bytes`, copied once otherwise.
from libc.stddef cimport size_t
from libc.stdint cimport int32_t, uint32_t, uint8_t

cdef extern from "mbedtls_py_threading.h":
    void mbedtls_py_threading_init()

cdef extern from "mbedtls/version.h":
    const char *mbedtls_version_get_string()

cdef extern from "psa/crypto.h":
    ctypedef int32_t psa_status_t
    ctypedef uint32_t psa_algorithm_t
    enum: PSA_SUCCESS
    psa_algorithm_t PSA_ALG_SHA_256
    psa_status_t psa_crypto_init()
    psa_status_t psa_hash_compute(psa_algorithm_t alg, const uint8_t *input, size_t input_length,
                                  uint8_t *hash, size_t hash_size, size_t *hash_length) nogil

# Both must run before any other Mbed TLS call. psa_crypto_init() is idempotent.
mbedtls_py_threading_init()
cdef psa_status_t _init_status = psa_crypto_init()
if _init_status != PSA_SUCCESS:
    raise ImportError(f"psa_crypto_init() failed with status {_init_status}")


def version():
    """The Mbed TLS version this extension was built against, e.g. ``"4.2.0"``."""
    return mbedtls_version_get_string().decode("ascii")


def sha256(data):
    """SHA-256 digest (32 bytes) of a bytes-like object. The GIL is released while hashing."""
    cdef bytes buf
    cdef uint8_t out[32]
    cdef size_t out_len = 0
    cdef psa_status_t status
    if type(data) is bytes:
        buf = data
    elif isinstance(data, (bytearray, memoryview)):
        buf = bytes(data)
    else:
        raise TypeError(f"sha256() argument must be bytes-like, not {type(data).__name__}")
    cdef const uint8_t *src = <const uint8_t *><char *>buf
    cdef size_t n = len(buf)
    with nogil:
        status = psa_hash_compute(PSA_ALG_SHA_256, src, n, out, 32, &out_len)
    if status != PSA_SUCCESS:
        raise RuntimeError(f"psa_hash_compute() failed with status {status}")
    return (<char *>out)[:out_len]
