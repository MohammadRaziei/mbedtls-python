# mbedtls-python

Python bindings for [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) (4.2.0), built the same way as
[httpp](https://github.com/MohammadRaziei/httpp): Cython, scikit-build-core, one `cp39-abi3` wheel per platform plus
separate free-threaded wheels (`cp313t`, `cp314t`, `cp315t`).

Mbed TLS is a git submodule, built from source and linked **statically** into the extension, with hidden visibility:
no system package, no DLL, and no symbol clash with another copy in the same process.

**Status: level 5** (1: build system, thread safety, CI; 2: digests, HMAC, random bytes; 3: installable C++ package; 4: release workflow; 5: X.509).

```python
import mbedtls

mbedtls.version()                       # "4.2.0"  (the Mbed TLS this wheel was built against)
mbedtls.algorithms                      # ("md5", "sha1", "sha224", "sha256", "sha384", "sha512", "sha3_224", ...)
mbedtls.digest("sha3_256", b"abc")      # bytes; names are spelled like hashlib's
mbedtls.sha256(b"abc")                  # shorthand for digest("sha256", ...)
mbedtls.hmac("sha256", key, data)       # key may be empty
mbedtls.random_bytes(32)                # PSA random generator
```

Input is `bytes`, `bytearray` or `memoryview` (anything else is a `TypeError`; an unknown algorithm is a `ValueError`).
The GIL is released while computing. Everything is one-shot and stateless, so there is nothing to lock on your side;
compare MACs with `hmac.compare_digest` from the standard library.

### X.509

```python
from mbedtls.x509 import Certificate, load_pem_bundle, VerificationError

leaf = Certificate.from_pem(pem_text)          # or Certificate.from_der(der_bytes)
leaf.subject, leaf.issuer, leaf.serial_number, leaf.not_before, leaf.not_after, leaf.is_ca
leaf.fingerprint()                             # sha256 of the DER, bytes

roots = load_pem_bundle(ca_bundle_pem)         # every certificate of a bundle
try:
    leaf.verify(roots, intermediates=[inter], hostname="example.com")
except VerificationError as e:
    print(e.reasons, e.flags)
```

Certificates are immutable. `verify` uses Mbed TLS's default profile (SHA-2 signatures, RSA >= 2048 bits, common
curves) and the current time; there is no CRL/OCSP support and no subjectAltName listing yet.

## Thread safety

TF-PSA-Crypto keeps process-wide state (the PSA random generator, key slots) that is only locked when its threading
layer is on, so it is always on (`cmake/mbedtls_config.h`): pthread on POSIX, an `SRWLOCK` alt layer on Windows
(`src/threading.cpp`). CI runs a C++ stress test under ThreadSanitizer with both layers.

## Build from a checkout

```bash
git clone --recurse-submodules https://github.com/MohammadRaziei/mbedtls-python
cd mbedtls-python
pip install -r requirements-dev.txt     # jsonschema + jinja2 for Mbed TLS's code generators; Perl must be on PATH

pip install .[test] && pytest           # the Python package
cmake -B build -DMBEDTLS_CPP_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build   # C++ core only
```

Forgot `--recurse-submodules`? `git submodule update --init --recursive`.

## Use the C++ core without building Mbed TLS

`cmake --install build --prefix <dir>` (or the `mbedtls-cpp-core-*` archive from the `prebuilt core` workflow) gives a
relocatable package: static libraries, headers and a CMake config.

```cmake
find_package(mbedtls_cpp CONFIG REQUIRED)        # CMAKE_PREFIX_PATH=<dir>
target_link_libraries(app PRIVATE mbedtls::cpp)
```

Call `mbedtls_cpp_threading_init()` before the first Mbed TLS / PSA call. `examples/consumer` is a complete project.
The target carries the include path, the exact user config the libraries were built with and the system libraries.

## Releasing

`.github/workflows/orchestrator.yml` runs the C++ tests (with ThreadSanitizer), the prebuilt package and all wheels.

- push to `master`: everything is built and the `__beta__` pre-release is refreshed with the artifacts.
- push of a tag `vX.Y.Z`: the same, then a GitHub release and PyPI. The tag must equal `__version__` in
  `src/bindings/python/mbedtls/__init__.py`, otherwise the run stops at the first step. Nothing is published unless every job passed.

Needs the repository secrets `PYPI_USERNAME` and `PYPI_PASSWORD` (same as httpp). Steps: bump `__version__`, commit,
`git tag vX.Y.Z && git push --tags`.

## Layout

| Path | What |
|---|---|
| `src/third_party/mbedtls` | submodule, pinned to `v4.2.0` |
| `cmake/mbedtls_config.h` | Mbed TLS user config (threading) |
| `include/`, `src/` | public headers and C++ core: `mbedtls::cpp` target, threading registration, digest/HMAC/X.509 helpers |
| `src/bindings/python/mbedtls/` | Python package and the Cython module `_mbedtls_cy.pyx` |
| `examples/consumer` | smallest user of the installed package (also run in CI) |
| `tests/python`, `tests/cpp` | `pytest` for Python, utest (`tests/utest`) for the core (ctest) |
| `tests/data` | test certificates, regenerate with `tests/gen_x509_fixtures.sh` |

## Use from C++ (via pip)

The wheel carries the static C++ core, the Mbed TLS headers and a CMake package config, so `pip install mbedtls` is
all a C++ project needs (same idea as `python -m nanobind --cmake_dir`):

```cmake
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} -m mbedtls --cmake-dir
                OUTPUT_VARIABLE mbedtls_cpp_DIR OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
find_package(mbedtls_cpp CONFIG REQUIRED)
target_link_libraries(app PRIVATE mbedtls::cpp)
```

```sh
mbedtls --cmake-dir     # also: --include-dir, --lib-dir (python -m mbedtls ... works too)
```

A complete project: `examples/find_mbedtls_via_python`.
