# mbedtls-python

Python bindings for [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) (4.2.0), built the same way as
[httpp](https://github.com/MohammadRaziei/httpp): Cython, scikit-build-core, one `cp39-abi3` wheel per platform plus
separate free-threaded wheels (`cp313t`, `cp314t`, `cp315t`).

Mbed TLS is a git submodule, built from source and linked statically into **one shared library, `mbedtls_cpp`**
(`libmbedtls_cpp.so`, `libmbedtls_cpp.dylib`, `mbedtls_cpp.dll`), with hidden visibility: the Python extension and every C++
program link to the same copy, only the C++ API of `include/mbedtls.hpp` is exported, and so there is no system package and no
symbol clash with another copy of Mbed TLS in the same process. The wheel carries it as `mbedtls/sdk/` (`include/`, `lib/`,
`lib/cmake/mbedtls_cpp/`).

**Status: level 5** (1: build system, thread safety, CI; 2: digests, HMAC, random bytes; 3: installable C++ package; 4: release workflow; 5: X.509).

```python
import mbedtls

mbedtls.__mbedtls_version__             # "4.2.0"  (the Mbed TLS this wheel was built against)
mbedtls.__version__                     # "0.1.0"  (this Python package)
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

CMake options: `MBEDTLS_CPP_BUILD_TESTS` (C++ tests, plus the Python and consumer tests under ctest), `MBEDTLS_CPP_BUILD_PYTHON`
(the extension, installed with the same `mbedtls/sdk` layout as the wheel into `build/python/install`),
`MBEDTLS_CPP_BUILD_EXAMPLES`, `MBEDTLS_CPP_THREADING_ALT`. The C++ tests compile the core into a static library, because
they call the Mbed TLS / PSA C API directly, which the shared library keeps hidden.

## Use the C++ core without Python

`cmake --install build --prefix <dir>` gives a relocatable package: the shared library, `mbedtls.hpp` and a CMake config.

```cmake
find_package(mbedtls_cpp CONFIG REQUIRED)    # CMAKE_PREFIX_PATH=<dir>
target_link_libraries(app PRIVATE mbedtls::cpp)
```

```cpp
#include <mbedtls.hpp>

std::string sum = mbedtls::digest("sha256", "abc");   // or mbedtls::Digest / Hmac, reusable objects
auto chain = mbedtls::Certificate::from_pem(pem);     // immutable values, no native handle to manage
auto result = chain[0].verify(trusted, intermediates, "example.org");   // result.ok / .flags / .info
```

Errors are exceptions: `UnknownAlgorithm` and `CertificateError` (`std::invalid_argument`), `Error`
(`std::runtime_error`, a failed PSA call). `mbedtls::init()` runs on first use. The Mbed TLS / PSA C API is not part of the
package (it stays hidden inside the library); `examples/consumer` is a complete project.

## Releasing

`.github/workflows/orchestrator.yml` runs the cmake workflow (C++, Python and consumer tests, ThreadSanitizer) and the wheels workflow.

- push to `master`: everything is built and the `__beta__` pre-release is refreshed with the artifacts.
- push of a tag `vX.Y.Z`: the same, then a GitHub release and PyPI. The tag must equal the `version` in `pyproject.toml`,
  otherwise the run stops at the first step. Nothing is published unless every job passed.

Needs the repository secrets `PYPI_USERNAME` and `PYPI_PASSWORD` (same as httpp). The version of this project lives in
one place, `version = "X.Y.Z"` in `pyproject.toml`: the wheel, CMake (`cmake/PyProject.cmake`, `PROJECT_VERSION`) and CI
all read it from there; `mbedtls.__version__` is that version, `mbedtls.__mbedtls_version__` (C++:
`mbedtls::version_string()` and `mbedtls::version_hex()`) is the version of Mbed TLS itself. Steps: change `version`,
commit, `git tag vX.Y.Z && git push --follow-tags`.

## Layout

| Path | What |
|---|---|
| `src/third_party/mbedtls` | submodule, pinned to `v4.2.0` |
| `cmake/` | `mbedtls_config.h` (Mbed TLS user config: threading), `mbedtls_cppConfig.cmake.in`, `PyProject`, `Startup`, `Optimize`, Cython helpers |
| `include/`, `src/mbedtls.cpp` | the C++ core: `include/mbedtls.hpp` + `include/mbedtls_cpp/export.hpp`, built as the shared library `mbedtls_cpp` (target `mbedtls::cpp`) |
| `src/bindings/python/` | its `CMakeLists.txt`, the Python package `mbedtls/` and the Cython module `_mbedtls_cy.pyx` |
| `examples/consumer` | smallest user of the installed package (built by ctest, both from a prefix and from the wheel) |
| `tests/python`, `tests/cpp`, `tests/cmake` | `pytest`, utest (`tests/utest`) for the core, and the `find_package` consumer tests (all under ctest) |
| `tests/data` | test certificates, regenerate with `tests/gen_x509_fixtures.sh` |

## Use from C++ (via pip)

The wheel carries the C++ SDK as `mbedtls/sdk/` (headers, the shared library, the CMake package), so `pip install mbedtls` is
all a C++ project needs (same idea as `python -m nanobind --cmake_dir`):

```cmake
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} -m mbedtls --cmake-dir
                OUTPUT_VARIABLE mbedtls_cpp_DIR OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
find_package(mbedtls_cpp CONFIG REQUIRED)
target_link_libraries(app PRIVATE mbedtls::cpp)
```

```sh
mbedtls --cmake-dir     # also: --sdk-dir, --include-dir, --lib-dir (python -m mbedtls ... works too)
```

On Windows the program finds `mbedtls_cpp.dll` only beside itself or on `PATH`; `examples/consumer/CMakeLists.txt` copies it.
