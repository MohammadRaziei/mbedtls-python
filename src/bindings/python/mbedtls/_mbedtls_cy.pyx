# cython: language_level=3, freethreading_compatible=True
# Stable-ABI (abi3, cp39) safe: no typed memoryviews (Py_buffer is only in the limited API from 3.11), no
# PyObject internals. Input is read in place when it is `bytes`, copied once otherwise. Everything here is stateless,
# so it is safe to call from any number of threads (the PSA state underneath is locked, see cmake/mbedtls_config.h).
from libc.stddef cimport size_t
from libc.stdint cimport int32_t, uint32_t, uint8_t
from libc.stdlib cimport free, malloc

cdef extern from "mbedtls_cpp_threading.h":
    void mbedtls_cpp_threading_init()

cdef extern from "mbedtls_cpp_crypto.h":
    enum: MBEDTLS_CPP_MAX_DIGEST_SIZE
    enum: MBEDTLS_CPP_ERR_UNKNOWN_ALGORITHM
    const char *mbedtls_cpp_hash_name(size_t i)
    int mbedtls_cpp_digest(const char *name, const uint8_t *data, size_t data_len, uint8_t *out,
                          size_t *out_len) nogil
    int mbedtls_cpp_hmac(const char *name, const uint8_t *key, size_t key_len, const uint8_t *data,
                        size_t data_len, uint8_t *out, size_t *out_len) nogil

cdef extern from "mbedtls_cpp_x509.h":
    enum: MBEDTLS_CPP_X509_NAME_MAX
    enum: MBEDTLS_CPP_X509_SERIAL_MAX
    ctypedef struct mbedtls_cpp_x509:
        pass
    ctypedef struct mbedtls_cpp_x509_info:
        const uint8_t *der
        size_t der_len
        int version
        int is_ca
        char subject[1024]
        char issuer[1024]
        uint8_t serial[64]
        size_t serial_len
        int not_before[6]
        int not_after[6]
    int mbedtls_cpp_x509_parse(const uint8_t *buf, size_t len, int pem, mbedtls_cpp_x509 **out) nogil
    void mbedtls_cpp_x509_free(mbedtls_cpp_x509 *chain) nogil
    size_t mbedtls_cpp_x509_count(const mbedtls_cpp_x509 *chain) nogil
    int mbedtls_cpp_x509_get_info(const mbedtls_cpp_x509 *chain, size_t i, mbedtls_cpp_x509_info *out) nogil
    int mbedtls_cpp_x509_verify(const uint8_t *const *chain, const size_t *chain_len, size_t chain_n,
                               const uint8_t *const *trust, const size_t *trust_len, size_t trust_n,
                               const char *hostname, uint32_t *flags, char *info, size_t info_size) nogil
    void mbedtls_cpp_x509_strerror(int code, char *buf, size_t size)

cdef extern from "mbedtls/version.h":
    const char *mbedtls_version_get_string()

cdef extern from "psa/crypto.h":
    ctypedef int32_t psa_status_t
    enum: PSA_SUCCESS
    psa_status_t psa_crypto_init()
    psa_status_t psa_generate_random(uint8_t *output, size_t output_size) nogil

# Both must run before any other Mbed TLS call. psa_crypto_init() is idempotent.
mbedtls_cpp_threading_init()
cdef psa_status_t _init_status = psa_crypto_init()
if _init_status != PSA_SUCCESS:
    raise ImportError(f"psa_crypto_init() failed with status {_init_status}")

cdef list _names = []
cdef size_t _i = 0
while mbedtls_cpp_hash_name(_i) != NULL:
    _names.append(mbedtls_cpp_hash_name(_i).decode("ascii"))
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
    if status == MBEDTLS_CPP_ERR_UNKNOWN_ALGORITHM:
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
    cdef uint8_t out[MBEDTLS_CPP_MAX_DIGEST_SIZE]
    cdef size_t out_len = 0
    cdef int status
    with nogil:
        status = mbedtls_cpp_digest(alg_c, src, n, out, &out_len)
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
    cdef uint8_t out[MBEDTLS_CPP_MAX_DIGEST_SIZE]
    cdef size_t out_len = 0
    cdef int status
    with nogil:
        status = mbedtls_cpp_hmac(alg_c, key_p, key_n, src, n, out, &out_len)
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


# --- X.509: the low-level half, the public API is mbedtls/x509.py ---

cdef str _x509_error(int code):
    cdef char text[256]
    text[0] = 0
    mbedtls_cpp_x509_strerror(code, text, sizeof(text))
    msg = (<bytes>text).decode("ascii", "replace")
    return f"{msg} [-0x{-code:04X}]" if code < 0 else msg


def _x509_parse(data, bint pem):
    """Parse PEM text (one or more certificates) or one DER certificate. Returns a list of
    (der, version, is_ca, subject, issuer, serial, not_before, not_after), the times as (y, m, d, H, M, S) in UTC."""
    cdef bytes buf = _as_bytes(data, "data")
    cdef const uint8_t *src = <const uint8_t *><char *>buf
    cdef size_t n = len(buf)
    cdef mbedtls_cpp_x509 *chain = NULL
    cdef mbedtls_cpp_x509_info info
    cdef int ret
    cdef size_t i, count
    with nogil:
        ret = mbedtls_cpp_x509_parse(src, n, pem, &chain)
    if ret != 0:
        raise ValueError(f"invalid certificate: {_x509_error(ret)}")
    out = []
    try:
        count = mbedtls_cpp_x509_count(chain)
        for i in range(count):
            ret = mbedtls_cpp_x509_get_info(chain, i, &info)
            if ret != 0:
                raise ValueError(f"invalid certificate: {_x509_error(ret)}")
            out.append((
                (<char *>info.der)[:info.der_len],
                info.version,
                info.is_ca != 0,
                (<bytes>info.subject).decode("utf-8", "replace"),
                (<bytes>info.issuer).decode("utf-8", "replace"),
                (<char *>info.serial)[:info.serial_len],
                tuple([info.not_before[k] for k in range(6)]),
                tuple([info.not_after[k] for k in range(6)]),
            ))
    finally:
        mbedtls_cpp_x509_free(chain)
    return out


def _x509_verify(list chain, list trust, hostname):
    """Verify DER ``chain[0]`` (the rest are intermediates) against the DER ``trust`` anchors. ``hostname`` is bytes
    or None. Returns (status, flags, text): status 0 = verified, 1 = did not verify (flags and text say why)."""
    cdef size_t cn = len(chain)
    cdef size_t tn = len(trust)
    cdef size_t i
    cdef bytes item
    cdef bytes host = hostname
    cdef const char *host_c = NULL
    cdef const uint8_t **ptrs = <const uint8_t **>malloc((cn + tn + 1) * sizeof(void *))
    cdef size_t *lens = <size_t *>malloc((cn + tn + 1) * sizeof(size_t))
    cdef char text[1024]
    cdef uint32_t flags = 0
    cdef int ret
    if ptrs == NULL or lens == NULL:
        free(ptrs)
        free(lens)
        raise MemoryError()
    text[0] = 0
    if host is not None:
        host_c = host
    try:
        # `chain` and `trust` keep every bytes object alive while the GIL is released below.
        for i in range(cn):
            item = chain[i]
            ptrs[i] = <const uint8_t *><char *>item
            lens[i] = len(item)
        for i in range(tn):
            item = trust[i]
            ptrs[cn + i] = <const uint8_t *><char *>item
            lens[cn + i] = len(item)
        with nogil:
            ret = mbedtls_cpp_x509_verify(ptrs, lens, cn, ptrs + cn, lens + cn, tn, host_c, &flags, text,
                                         sizeof(text))
    finally:
        free(ptrs)
        free(lens)
    if ret == 0:
        return (0, 0, "")
    if ret == 1:
        return (1, flags, (<bytes>text).decode("utf-8", "replace"))
    raise ValueError(f"invalid certificate: {_x509_error(ret)}")
