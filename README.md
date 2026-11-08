# mbedtls-python

Python bindings for [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) (4.2.0), built the same way as
[httpp](https://github.com/MohammadRaziei/httpp): Cython, scikit-build-core, one `cp39-abi3` wheel per platform plus
separate free-threaded wheels (`cp313t`, `cp314t`, `cp315t`).

Mbed TLS is a git submodule, built from source as **shared libraries** (static on Windows: Mbed TLS cannot be a DLL there) and installed whole into the SDK: `include/mbedtls`,
`include/psa`, `libmbedtls`, `libmbedx509`, `libtfpsacrypto` and `lib/cmake/MbedTLS` (`find_package(MbedTLS)` gives `MbedTLS::mbedtls`,
`MbedTLS::mbedx509`, `MbedTLS::tfpsacrypto`), written by Mbed TLS's own install rules. The wheel carries that install as `mbedtls/sdk/`,
so there is no system package: Python and any C/C++ project use the same libraries. Nothing is wrapped around the C API; the
Python module is a thin Cython layer (with a small private C++ shim, `src/bindings/python/shim`, compiled into the extension only).

**Status:** hashes, HMAC, random bytes and X.509 in Python; the full Mbed TLS (including TLS) as the SDK.

```python
import mbedtls

mbedtls.__mbedtls_version__             # "4.2.0"  (the Mbed TLS this wheel was built against)
mbedtls.__version__                     # "0.3.0"  (this Python package)
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
(`src/mbedtls.cpp`). CI runs a C++ stress test under ThreadSanitizer with both layers.

## Build from a checkout

```bash
git clone --recurse-submodules https://github.com/MohammadRaziei/mbedtls-python
cd mbedtls-python
pip install -r requirements-dev.txt     # jsonschema + jinja2 for Mbed TLS's code generators, cython, pytest; Perl must be on PATH

pip install .[test] && pytest           # the Python package (the wheel layout)
cmake -S . -B build && cmake --build build && ctest --test-dir build   # everything: core, extension, tests
```

Forgot `--recurse-submodules`? `git submodule update --init --recursive`.

CMake options: `MBEDTLS_CPP_BUILD_TESTS` (tests of the Python shim, plus the Python and consumer tests under ctest), `MBEDTLS_CPP_BUILD_PYTHON`
(the extension, installed with the same `mbedtls/sdk` layout as the wheel into `build/python/install`),
`MBEDTLS_CPP_BUILD_EXAMPLES`, `MBEDTLS_CPP_THREADING_ALT`.

## Use Mbed TLS from C / C++ without Python

`cmake --install build --prefix <dir>` (or the `mbedtls-*` archive from the `prebuilt core` workflow) installs Mbed TLS itself:

```cmake
find_package(MbedTLS CONFIG REQUIRED)    # CMAKE_PREFIX_PATH=<dir>
target_link_libraries(app PRIVATE MbedTLS::mbedtls MbedTLS::mbedx509 MbedTLS::tfpsacrypto)
```

The installed headers carry the project's configuration (`mbedtls_python_config.h`, installed beside them, thread safety on): use
the headers of this install, not another Mbed TLS's. On POSIX the pthread threading layer needs nothing; where the alt layer is used
(Windows) the application registers it once with `mbedtls_threading_set_alt()` before any other call: `examples/consumer/main.c`
shows how. Another copy of Mbed TLS in the same process (the system's, another wheel) is not isolated from this one: the libraries
export their normal symbols.

## Layout

| Path | What |
|---|---|
| `src/third_party/mbedtls` | submodule, pinned to `v4.2.0` |
| `cmake/` | `mbedtls_config.h` (Mbed TLS user config: threading, installed with the headers), `PyProject`, `Startup`, `Optimize`, Cython helpers |
| `src/alt/` | `threading_alt.h`, the alt threading layer's types (Windows) |
| `src/bindings/python/` | its `CMakeLists.txt`, the Python package `mbedtls/`, the Cython module `_mbedtls_cy.pyx` and `shim/` (private C++ over the C API) |
| `examples/consumer`, `examples/find_mbedtls_via_python` | smallest users of the installed package, from a prefix and from the wheel (both built by ctest) |
| `tests/python`, `tests/cpp`, `tests/cmake` | `pytest`, utest (`tests/utest`) for the shim, and the `find_package` consumer tests (all under ctest) |
| `tests/data` | test certificates, regenerate with `tests/gen_x509_fixtures.sh` |

## Use from C / C++ (via pip)

The wheel carries Mbed TLS as `mbedtls/sdk/` (headers, shared libraries, the CMake package), so `pip install mbedtls` is
all a C or C++ project needs (same idea as `python -m nanobind --cmake_dir`):

```cmake
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} -m mbedtls --cmake-dir
                OUTPUT_VARIABLE MbedTLS_DIR OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
find_package(MbedTLS CONFIG REQUIRED)
target_link_libraries(app PRIVATE MbedTLS::tfpsacrypto)    # and MbedTLS::mbedtls / MbedTLS::mbedx509 for TLS and X.509
```

```sh
mbedtls --cmake-dir     # also: --sdk-dir, --include-dir, --lib-dir (python -m mbedtls ... works too)
```

On Windows the SDK libraries are static (`.lib`), nothing to copy next to the program. A complete project: `examples/find_mbedtls_via_python`.

Not verified: this layout was written without a build. Run the configure, `pip wheel .` and `ctest`, and check in particular that the
installed `MbedTLSTargets.cmake` points at `mbedtls_python_config.h` and not at a build directory.
