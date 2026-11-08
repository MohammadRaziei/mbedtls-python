"""Python bindings for Mbed TLS: thread-safe, one abi3 wheel for CPython 3.9+, with Mbed TLS itself (shared libraries, headers, CMake package) inside, as the SDK."""

import os as _os

try:
    # This Python package's own version: the one in the wheel's metadata, written from pyproject.toml. Never hardcoded
    # here, so it cannot drift from the release. (__mbedtls_version__, below, is the Mbed TLS it contains.)
    from importlib.metadata import version as _pkg_version

    __version__ = _pkg_version("mbedtls")
except Exception:
    __version__ = "0.0.0+unknown"

_pkg_dir = _os.path.dirname(_os.path.abspath(__file__))

# The SDK is an install prefix of Mbed TLS itself inside the package, mbedtls/sdk/: include/mbedtls, include/psa, the
# shared libraries in lib/ (libmbedtls, libmbedx509, libtfpsacrypto: found through the extension's rpath) and
# lib/cmake/MbedTLS (find_package(MbedTLS)). Windows has no rpath and does not search PATH for the DLLs of an extension module,
# so the directory is added by hand, before the first import of the extension below. os.add_dll_directory exists only
# there: asking for the function, not the platform, is the check.
_sdk_dir = _os.path.join(_pkg_dir, "sdk")
_lib_dir = _os.path.join(_sdk_dir, "lib")
if hasattr(_os, "add_dll_directory") and _os.path.isdir(_lib_dir):  # pragma: no cover (Windows only)
    _dll_directory = _os.add_dll_directory(_lib_dir)  # kept: it stays registered for the process


def get_sdk_dir() -> str:
    """Root of the Mbed TLS install inside the package: an install prefix with include/ and lib/."""
    return _sdk_dir


def get_include_dir() -> str:
    """Directory with the Mbed TLS headers: mbedtls/, psa/ (<sdk>/include)."""
    return _os.path.join(_sdk_dir, "include")


def get_lib_dir() -> str:
    """Directory with the Mbed TLS shared libraries: libmbedtls, libmbedx509, libtfpsacrypto (<sdk>/lib)."""
    return _lib_dir


def get_cmake_dir() -> str:
    """Directory with MbedTLSConfig.cmake: use as MbedTLS_DIR, then link MbedTLS::mbedtls etc. (<sdk>/lib/cmake/MbedTLS)."""
    return _os.path.join(_lib_dir, "cmake", "MbedTLS")


from . import x509
from ._mbedtls_cy import __mbedtls_version__, algorithms, digest, hmac, random_bytes, sha256

__all__ = ["__mbedtls_version__", "__version__", "algorithms", "digest", "get_cmake_dir", "get_include_dir", "get_lib_dir",
           "get_sdk_dir", "hmac", "random_bytes", "sha256", "x509"]
