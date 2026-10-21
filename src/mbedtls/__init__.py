"""Python bindings for Mbed TLS: statically linked, thread-safe, one abi3 wheel for CPython 3.9+."""

__version__ = "0.1.0"

from ._mbedtls import sha256, version

__all__ = ["__version__", "sha256", "version"]
