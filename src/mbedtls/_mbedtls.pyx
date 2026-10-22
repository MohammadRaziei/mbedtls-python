# cython: language_level=3, freethreading_compatible=True
# Stable-ABI (abi3, cp39) safe: no typed memoryviews (Py_buffer is only in the limited API from 3.11), no
# PyObject internals. Input is read in place when it is `bytes`, copied once otherwise. Everything here is stateless,
# so it is safe to call from any number of threads (the PSA state underneath is locked, see cmake/mbedtls_config.h).
from libc.stddef cimport size_t
from libc.stdint cimport int32_t, uint32_t, uint8_t
from libc.stdlib cimport free, malloc

cdef extern from "mbedtls_py_threading.h":
    void mbedtls_py_threading_init()

cdef extern from "mbedtls_py_crypto.h":
    enum: MBEDTLS_PY_MAX_DIGEST_SIZE
    enum: MBEDTLS_PY_ERR_UNKNOWN_ALGORITHM
    const char *mbedtls_py_hash_name(size_t i)
    int mbedtls_py_digest(const char *name, const uint8_t *data, size_t data_len, uint8_t *out,
                          size_t *out_len) nogil
    int mbedtls_py_hmac(const char *name, const uint8_t *key, size_t key_len, const uint8_t *data,
                        size_t data_len, uint8_t *out, size_t *out_len) nogil

cdef extern from "mbedtls/version.h":
    const char *mbedtls_version_get_string()

cdef extern from "psa/crypto.h":
    ctypedef int32_t psa_status_t
    enum: PSA_SUCCESS
    psa_status_t psa_crypto_init()
    psa_status_t psa_generate_random(uint8_t *output, size_t output_size) nogil

# Both must run before any other Mbed TLS call. psa_crypto_init() is idempotent.
mbedtls_py_threading_init()
cdef psa_status_t _init_status = psa_crypto_init()
if _init_status != PSA_SUCCESS:
    raise ImportError(f"psa_crypto_init() failed with status {_init_status}")

cdef list _names = []
cdef size_t _i = 0
while mbedtls_py_hash_name(_i) != NULL:
    _names.append(mbedtls_py_hash_name(_i).decode("ascii"))
    _i += 1
algorithms = tuple(_names)
"""Names of the supported hash algorithms, spelled like :mod:`hashlib` does."""


cdef bytes _as_bytes(obj, what):
    if type(obj) is bytes:  # read in place; a `cdef bytes` accepts the exact type only
        return obj
    if isinstance(obj, (bytes, bytearray, memoryview)):  # subclass or buffer: copy once
        return bytes(obj)
    raise TypeError(f"{what} must be bytes-like, not {type(obj).__name__}")


cdef bytes _algorithm_name(name):
    if not isinstance(name, str):
        raise TypeError(f"algorithm name must be str, not {type(name).__name__}")
    try:
        encoded = name.encode("ascii")
    except UnicodeEncodeError:
        raise ValueError(f"unsupported hash algorithm: {name!r}") from None
    if b"\x00" in encoded:  # the C side compares NUL-terminated strings: "sha256\0junk" would match "sha256"
        raise ValueError(f"unsupported hash algorithm: {name!r}")
    return encoded


cdef _check(int status, name, what):
    if status == MBEDTLS_PY_ERR_UNKNOWN_ALGORITHM:
        raise ValueError(f"unsupported hash algorithm: {name!r}")
    if status != 0:
        raise RuntimeError(f"{what} failed with PSA status {status}")


def version():
    """The Mbed TLS version this extension was built against, e.g. ``"4.2.0"``."""
    return mbedtls_version_get_string().decode("ascii")


def digest(name, data):
    """Digest of a bytes-like object. ``name`` is one of :data:`algorithms`. The GIL is released while hashing."""
    cdef bytes alg = _algorithm_name(name)
    cdef bytes buf = _as_bytes(data, "data")
    cdef const char *alg_c = alg
    cdef const uint8_t *src = <const uint8_t *><char *>buf
    cdef size_t n = len(buf)
    cdef uint8_t out[MBEDTLS_PY_MAX_DIGEST_SIZE]
    cdef size_t out_len = 0
    cdef int status
    with nogil:
        status = mbedtls_py_digest(alg_c, src, n, out, &out_len)
    _check(status, name, "digest")
    return (<char *>out)[:out_len]


def sha256(data):
    """SHA-256 digest (32 bytes); shorthand for ``digest("sha256", data)``."""
    return digest("sha256", data)


def hmac(name, key, data):
    """HMAC of ``data`` under ``key`` (both bytes-like, ``key`` may be empty), hashed with ``name``.
    The GIL is released while computing. To compare two MACs use :func:`hmac.compare_digest` from the standard library."""
    cdef bytes alg = _algorithm_name(name)
    cdef bytes k = _as_bytes(key, "key")
    cdef bytes buf = _as_bytes(data, "data")
    cdef const char *alg_c = alg
    cdef const uint8_t *key_p = <const uint8_t *><char *>k
    cdef const uint8_t *src = <const uint8_t *><char *>buf
    cdef size_t key_n = len(k)
    cdef size_t n = len(buf)
    cdef uint8_t out[MBEDTLS_PY_MAX_DIGEST_SIZE]
    cdef size_t out_len = 0
    cdef int status
    with nogil:
        status = mbedtls_py_hmac(alg_c, key_p, key_n, src, n, out, &out_len)
    _check(status, name, "hmac")
    return (<char *>out)[:out_len]


def random_bytes(n):
    """``n`` cryptographically secure random bytes from the PSA random generator (the GIL is released)."""
    cdef Py_ssize_t count
    cdef uint8_t *buf
    cdef psa_status_t status
    if not isinstance(n, int) or isinstance(n, bool):
        raise TypeError(f"n must be int, not {type(n).__name__}")
    count = n  # OverflowError if it does not fit a Py_ssize_t
    if count < 0:
        raise ValueError("n must be >= 0")
    if count == 0:
        return b""
    buf = <uint8_t *>malloc(<size_t>count)
    if buf == NULL:
        raise MemoryError()
    try:
        with nogil:
            status = psa_generate_random(buf, <size_t>count)
        if status != PSA_SUCCESS:
            raise RuntimeError(f"random_bytes failed with PSA status {status}")
        # ponytail: one extra copy (malloc buffer -> bytes). Upgrade path: fill the bytes object in place, which
        # needs PyBytes_AS_STRING and so is not in the limited API; PyBytes_AsString after creation would work.
        return (<char *>buf)[:count]
    finally:
        free(buf)
