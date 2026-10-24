# Changelog

All notable changes to mbedtls-python are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project follows
[Semantic Versioning](https://semver.org/) (while the major version is 0, a minor release may still change an API;
such changes are listed under **Changed**). The version of the project is the `version` in `pyproject.toml`;
`mbedtls.__mbedtls_version__` is the version of Mbed TLS inside it.

## [Unreleased]

## [0.1.0] - 2026-10-24

First release. Python bindings for Mbed TLS 4.2.0, linked statically into the extension (no shared library to ship).

### Added
- **Hashes, HMAC and random bytes:** `mbedtls.digest(name, data)`, `sha256(data)`, `hmac(name, key, data)`,
  `random_bytes(n)` and `algorithms` (md5, sha1, sha224/256/384/512, sha3_224/256/384/512, spelled like `hashlib`). The GIL is
  released while hashing.
- **X.509:** `mbedtls.x509.Certificate` (immutable; `from_pem`, `from_der`, `to_pem`, `fingerprint`, subject, issuer, serial,
  validity as UTC `datetime`, `is_ca`, `version`), `load_pem_bundle(...)` and `cert.verify(trusted, intermediates=(),
  hostname=None)`, which raises `VerificationError` (a `ValueError`, with `.reasons` and `.flags`). Verification uses Mbed TLS's
  default profile (SHA-2, RSA from 2048 bits); no CRL and no OCSP.
- **Thread safety:** Mbed TLS's shared state (the PSA random generator, key slots) is locked through its threading hooks: pthreads on
  Linux and macOS, SRWLOCK on Windows. The extension declares itself free-threading compatible (`freethreading_compatible`) and
  keeps no mutable state of its own.
- **Wheels:** one `cp39-abi3` wheel per platform for every GIL-enabled CPython from 3.9, and free-threaded wheels for 3.13t, 3.14t and
  3.15t.
- **The C++ layer, usable without Python:** `include/mbedtls.hpp` with `mbedtls::Digest`, `Hmac`, `Certificate`, `init()`, exceptions
  derived from `std::invalid_argument` / `std::runtime_error`, and `mbedtls::version_string()` / `version_hex()` (the version of
  Mbed TLS itself).
- **Use it from a C++ project:** `pip install mbedtls`, then `python -m mbedtls --cmake-dir` (also the `mbedtls` command, with
  `--include-dir` and `--lib-dir`) points `find_package(MbedTLS CONFIG)` at the installed package. The config is a drop-in for the one
  Mbed TLS installs itself (`MbedTLS::mbedtls`, `MbedTLS::mbedx509`, `MbedTLS::tfpsacrypto`, version 4.2.0) and adds the target
  `mbedtls::cpp`, which also asks for C++17. The same package is released as a prebuilt archive per OS and architecture.
- **Versions:** `mbedtls.__version__` (this package) and `mbedtls.__mbedtls_version__` (Mbed TLS).
- **Release pipeline:** `.github/workflows/orchestrator.yml` runs the C++ tests (also under ThreadSanitizer), the prebuilt package and
  all wheels, and publishes only when every job passed. Pushing a tag `vX.Y.Z` that equals `version` in `pyproject.toml` creates the
  GitHub release and uploads to PyPI.

### Known limitations
- The import name `mbedtls` is also used by the `python-mbedtls` package; do not install both.
- `Certificate` does not list the subjectAltName entries (they are only used for the hostname check), and verification rejects SHA-1
  certificates.
- A parse error can read `UNKNOWN ERROR CODE (0062)` because Mbed TLS's `strerror` does not know some ASN.1 codes; the numeric
  code is shown next to it.
- Tested on Linux with Python 3.12. The workflows have not run on GitHub yet; Windows, macOS and CPython 3.9 are untested.

[Unreleased]: https://github.com/MohammadRaziei/mbedtls-python/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/MohammadRaziei/mbedtls-python/releases/tag/v0.1.0
