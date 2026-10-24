"""Python bindings for Mbed TLS: statically linked, thread-safe, one abi3 wheel for CPython 3.9+."""

__version__ = "0.1.0"

import os as _os

_pkg_dir = _os.path.dirname(_os.path.abspath(__file__))


def get_cmake_dir() -> str:
    """Directory with MbedTLSConfig.cmake: use as MbedTLS_DIR, then link mbedtls::cpp (or MbedTLS::mbedtls)."""
    return _os.path.join(_pkg_dir, "cmake")


def get_include_dir() -> str:
    """Directory with the Mbed TLS / PSA headers and mbedtls_cpp_*.h."""
    return _os.path.join(_pkg_dir, "include")


def get_lib_dir() -> str:
    """Directory with the static libraries (mbedtls_cpp_core, mbedtls, mbedx509, tfpsacrypto)."""
    return _os.path.join(_pkg_dir, "lib")


from . import x509
from ._mbedtls_cy import algorithms, digest, hmac, random_bytes, sha256, version

__all__ = ["__version__", "algorithms", "digest", "get_cmake_dir", "get_include_dir", "get_lib_dir", "hmac",
           "random_bytes", "sha256", "version", "x509"]
