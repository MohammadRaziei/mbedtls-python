# Changelog

All notable changes to mbedtls-python are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project follows
[Semantic Versioning](https://semver.org/) (while the major version is 0, a minor release may still change an API;
such changes are listed under **Changed**). The version of the project is the `version` in `pyproject.toml`;
`mbedtls.__mbedtls_version__` is the version of Mbed TLS inside it.

## [Unreleased]

### Changed
- **The SDK is Mbed TLS itself.** It is built as shared libraries and installed whole by Mbed TLS's own install rules: `include/mbedtls`,
  `include/psa`, `libmbedtls` / `libmbedx509` / `libtfpsacrypto`, `lib/cmake/MbedTLS` (`MbedTLS::mbedtls`, `MbedTLS::mbedx509`,
  `MbedTLS::tfpsacrypto`). The wheel carries it as `mbedtls/sdk/`. The project's user config (thread safety) is installed beside the
  headers as `mbedtls_python_config.h`, and the exported targets point at it.
- **`mbedtls_cpp_core`, `mbedtls::cpp`, `mbedtls.hpp` and `export.hpp` are gone.** The C++ classes are now a private shim inside the Python
  extension (`src/bindings/python/shim`), not installed. Hidden visibility is gone too: another copy of Mbed TLS in the same process is
  no longer isolated.
- `examples/consumer` is a C program on `MbedTLS::tfpsacrypto`; the prebuilt archive is `mbedtls-*`.
- Where the alt threading layer is used (Windows), an application using the SDK registers it itself (`mbedtls_threading_set_alt()`).
- Windows: Mbed TLS is installed as **static** libraries (`mbedtls.lib`, `mbedx509.lib`, `tfpsacrypto.lib`) and linked into the extension. It has no export macros, and `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS` misses its data symbols (`mbedtls_x509_crt_profile_default`, ...), so its DLLs cannot be linked.
- Linux/macOS wheels: the installed libraries carry `RUNPATH $ORIGIN` / `@loader_path`, so `auditwheel repair` can resolve `libmbedx509.so` from `libmbedtls.so`.

## [0.2.0] - 2026-11-07

The C++ core becomes a shared library and the wheel carries it as a ready-to-use SDK. While the major version is 0 this is a
minor release: the CMake package changes, see **Changed**.

### Added
- **`mbedtls/sdk/` in every wheel:** `include/`, `lib/` (the shared library) and `lib/cmake/MbedTLS/`, the same tree `cmake --install`
  makes. `mbedtls.get_sdk_dir()` and `python -m mbedtls --sdk-dir` print its root; `get_include_dir()`, `get_lib_dir()` and
  `get_cmake_dir()` point into it, so a C++ project needs only `pip install mbedtls` and `python -m mbedtls --cmake-dir`.
- **Export header** `include/mbedtls_cpp/export.hpp` (`MBEDTLS_CPP_API`): the classes and functions of `include/mbedtls.hpp` are exported
  from the shared library, everything else stays hidden.
- **CMake options** `MBEDTLS_CPP_BUILD_PYTHON` and `MBEDTLS_CPP_BUILD_EXAMPLES` next to `MBEDTLS_CPP_BUILD_TESTS`. With Python on, `cmake --build`
  also builds the extension (target `build_python`) and installs the package with the wheel's layout into `build/python/install`.
- **Consumer tests under `ctest`:** `tests/python` runs `pytest` against that package, `tests/cmake` builds `examples/find_mbedtls_via_python`
  against it and `examples/consumer` against a `cmake --install` prefix, and runs both.

### Changed
- **The C++ core is a shared library**, `mbedtls_cpp_core` (`libmbedtls_cpp_core.so`, `.dylib`, `mbedtls_cpp_core.dll`; target
  `mbedtls::cpp` as before), with Mbed TLS linked statically inside it and hidden. The Python extension and C++ programs share one copy,
  and no Mbed TLS symbol leaks, so another copy in the same process cannot clash.
- **CMake package:** `find_package(MbedTLS CONFIG)` and `mbedtls::cpp` work as before, but the package no longer provides `MbedTLS::mbedtls`,
  `MbedTLS::mbedx509` and `MbedTLS::tfpsacrypto` and no longer ships the Mbed TLS / PSA headers, because the shared library does not export
  the C API. The prebuilt `mbedtls-cpp-core-*` archives now contain the shared library. On Windows a program needs `mbedtls_cpp_core.dll`
  beside it (the examples copy it).
- **Build system and CI after libstb's structure:** `Startup` and `Optimize` modules, the Python part in `src/bindings/python/CMakeLists.txt`,
  `cmake.yml` and `wheels.yml` in libstb's shape (the wheels job no longer stops at the first failing platform). The C++ tests compile the core
  into a static library, because they call the Mbed TLS / PSA C API directly.
- `requirements-dev.txt` adds `cython` (needed by `build_python`).

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

[Unreleased]: https://github.com/MohammadRaziei/mbedtls-python/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/MohammadRaziei/mbedtls-python/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/MohammadRaziei/mbedtls-python/releases/tag/v0.1.0
