# cython: language_level=3, freethreading_compatible=True
# Stable-ABI (abi3, cp39) safe: no typed memoryviews (Py_buffer is only in the limited API from 3.11), no
# PyObject internals. Input is read in place when it is `bytes`, copied once otherwise. This module is a thin layer over
# the C++ classes in include/mbedtls.hpp: C++ exceptions become Python ones (std::invalid_argument -> ValueError,
# std::runtime_error -> RuntimeError). Everything here is stateless, so it is safe to call from any number of threads
# (the PSA state underneath is locked, see cmake/mbedtls_config.h).
from libc.stddef cimport size_t
from libc.stdint cimport uint32_t
from libcpp cimport bool as cppbool  # not `bool`: the Python builtin is used below
from libcpp.string cimport string
from libcpp.vector cimport vector

cdef extern from "<string_view>" namespace "std" nogil:
    cdef cppclass string_view:
        string_view()
        string_view(const char *, size_t)

cdef extern from "mbedtls.hpp" namespace "mbedtls":
    string cpp_version_string "mbedtls::version_string"() except +
    void cpp_init "mbedtls::init"() except +

cdef extern from "mbedtls.hpp" namespace "mbedtls":
    vector[string] cpp_hash_algorithms "mbedtls::hash_algorithms"() except +
    string cpp_digest "mbedtls::digest"(string_view name, string_view data) except + nogil
    string cpp_hmac "mbedtls::hmac"(string_view name, string_view key, string_view data) except + nogil
    string cpp_random_bytes "mbedtls::random_bytes"(size_t n) except + nogil

cdef extern from "mbedtls.hpp" namespace "mbedtls":
    cdef cppclass Time:
        int year, month, day, hour, minute, second
    cdef cppclass VerifyResult:
        cppbool ok
        uint32_t flags
        string info
    cdef cppclass Certificate:
        Certificate()
        @staticmethod
        Certificate from_der(string_view der) except + nogil
        @staticmethod
        vector[Certificate] from_pem(string_view pem) except + nogil
        const string &der()
        int version()
        cppbool is_ca()
        const string &subject()
        const string &issuer()
        const string &serial()
        const Time &not_before()
        const Time &not_after()
        VerifyResult verify(const vector[Certificate] &trusted, const vector[Certificate] &intermediates) except + nogil
        VerifyResult verify(const vector[Certificate] &trusted, const vector[Certificate] &intermediates,
                            string_view hostname) except + nogil

# Threading layer + psa_crypto_init(), before any other Mbed TLS call (the C++ side would also do it on first use).
try:
    cpp_init()
except Exception as _exc:
    raise ImportError(f"Mbed TLS initialisation failed: {_exc}") from None

algorithms = tuple([n.decode("ascii") for n in cpp_hash_algorithms()])
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
        return name.encode("ascii")
    except UnicodeEncodeError:
        raise ValueError(f"unsupported hash algorithm: {name!r}") from None


cdef bytes _bytes(const string &s):
    return s  # std::string -> bytes (a `<bytes>` cast would reinterpret the C++ object)


cdef str _text(const string &s):
    return _bytes(s).decode("utf-8", "replace")


cdef string _mbedtls_version = cpp_version_string()
__mbedtls_version__ = _bytes(_mbedtls_version).decode("ascii")
"""The Mbed TLS version this extension was built against, e.g. ``"4.2.0"`` (``__version__`` of the package is this
Python package's own version)."""


cdef inline string_view _view(bytes b):
    return string_view(<const char *>b, len(b))


def digest(name, data):
    """Digest of a bytes-like object. ``name`` is one of :data:`algorithms`. The GIL is released while hashing."""
    cdef bytes alg = _algorithm_name(name)
    cdef bytes buf = _as_bytes(data, "data")
    cdef string_view a = _view(alg)
    cdef string_view d = _view(buf)
    cdef string out
    with nogil:
        out = cpp_digest(a, d)
    return out


def sha256(data):
    """SHA-256 digest (32 bytes); shorthand for ``digest("sha256", data)``."""
    return digest("sha256", data)


def hmac(name, key, data):
    """HMAC of ``data`` under ``key`` (both bytes-like, ``key`` may be empty), hashed with ``name``.
    The GIL is released while computing. To compare two MACs use :func:`hmac.compare_digest` from the standard library."""
    cdef bytes alg = _algorithm_name(name)
    cdef bytes k = _as_bytes(key, "key")
    cdef bytes buf = _as_bytes(data, "data")
    cdef string_view a = _view(alg)
    cdef string_view kv = _view(k)
    cdef string_view d = _view(buf)
    cdef string out
    with nogil:
        out = cpp_hmac(a, kv, d)
    return out


def random_bytes(n):
    """``n`` cryptographically secure random bytes from the PSA random generator (the GIL is released)."""
    cdef Py_ssize_t count
    cdef string out
    if not isinstance(n, int) or isinstance(n, bool):
        raise TypeError(f"n must be int, not {type(n).__name__}")
    count = n  # OverflowError if it does not fit a Py_ssize_t
    if count < 0:
        raise ValueError("n must be >= 0")
    with nogil:
        out = cpp_random_bytes(<size_t>count)
    return out


# --- X.509: the low-level half, the public API is mbedtls/x509.py ---

def _x509_parse(data, bint pem):
    """Parse PEM text (one or more certificates) or one DER certificate. Returns a list of
    (der, version, is_ca, subject, issuer, serial, not_before, not_after), the times as (y, m, d, H, M, S) in UTC."""
    cdef bytes buf = _as_bytes(data, "data")
    cdef string_view sv = _view(buf)
    cdef vector[Certificate] certs
    cdef const Certificate *c
    cdef size_t i
    with nogil:
        if pem:
            certs = Certificate.from_pem(sv)
        else:
            certs.push_back(Certificate.from_der(sv))
    out = []
    for i in range(certs.size()):
        c = &certs[i]
        out.append((
            _bytes(c.der()),
            c.version(),
            c.is_ca(),
            _text(c.subject()),
            _text(c.issuer()),
            _bytes(c.serial()),
            (c.not_before().year, c.not_before().month, c.not_before().day,
             c.not_before().hour, c.not_before().minute, c.not_before().second),
            (c.not_after().year, c.not_after().month, c.not_after().day,
             c.not_after().hour, c.not_after().minute, c.not_after().second),
        ))
    return out


def _x509_verify(list chain, list trust, hostname):
    """Verify DER ``chain[0]`` (the rest are intermediates) against the DER ``trust`` anchors. ``hostname`` is bytes
    or None. Returns (status, flags, text): status 0 = verified, 1 = did not verify (flags and text say why)."""
    cdef Certificate leaf
    cdef vector[Certificate] intermediates, anchors
    cdef VerifyResult result
    cdef bytes item
    cdef bytes host
    cdef string_view sv
    cdef size_t i
    if len(chain) == 0:
        raise ValueError("chain must hold at least one certificate")
    # The Certificate objects hold copies, so the bytes objects only need to live through each from_der() call.
    item = chain[0]
    sv = _view(item)
    leaf = Certificate.from_der(sv)
    for i in range(1, len(chain)):
        item = chain[i]
        sv = _view(item)
        intermediates.push_back(Certificate.from_der(sv))
    for i in range(len(trust)):
        item = trust[i]
        sv = _view(item)
        anchors.push_back(Certificate.from_der(sv))
    if hostname is None:
        with nogil:
            result = leaf.verify(anchors, intermediates)
    else:
        host = hostname
        sv = _view(host)
        with nogil:
            result = leaf.verify(anchors, intermediates, sv)
    if result.ok:
        return (0, 0, "")
    return (1, result.flags, _text(result.info))
