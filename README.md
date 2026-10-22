# mbedtls-python

Python bindings for [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) (4.2.0), built the same way as
[httpp](https://github.com/MohammadRaziei/httpp): Cython, scikit-build-core, one `cp39-abi3` wheel per platform plus
separate free-threaded wheels (`cp313t`, `cp314t`, `cp315t`).

Mbed TLS is a git submodule, built from source and linked **statically** into the extension, with hidden visibility:
no system package, no DLL, and no symbol clash with another copy in the same process.

**Status: level 3** (1: build system, thread safety, CI; 2: one-shot digests, HMAC, random bytes; 3: installable C++ package).

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

## Thread safety

TF-PSA-Crypto keeps process-wide state (the PSA random generator, key slots) that is only locked when its threading
layer is on, so it is always on (`cmake/mbedtls_config.h`): pthread on POSIX, an `SRWLOCK` alt layer on Windows
(`src/core/threading.cpp`). CI runs a C++ stress test under ThreadSanitizer with both layers.

## Build from a checkout

```bash
git clone --recurse-submodules https://github.com/MohammadRaziei/mbedtls-python
cd mbedtls-python
pip install -r requirements-dev.txt     # jsonschema + jinja2 for Mbed TLS's code generators; Perl must be on PATH

pip install .[test] && pytest           # the Python package
cmake -B build -DMBEDTLS_PY_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build   # C++ core only
```

Forgot `--recurse-submodules`? `git submodule update --init --recursive`.

## Use the C++ core without building Mbed TLS

`cmake --install build --prefix <dir>` (or the `mbedtls-py-core-*` archive from the `prebuilt core` workflow) gives a
relocatable package: static libraries, headers and a CMake config.

```cmake
find_package(mbedtls_py CONFIG REQUIRED)        # CMAKE_PREFIX_PATH=<dir>
target_link_libraries(app PRIVATE mbedtls_py::core)
```

Call `mbedtls_py_threading_init()` before the first Mbed TLS / PSA call. `examples/consumer` is a complete project.
The target carries the include path, the exact user config the libraries were built with and the system libraries.

## Layout

| Path | What |
|---|---|
| `src/third_party/mbedtls` | submodule, pinned to `v4.2.0` |
| `cmake/mbedtls_config.h` | Mbed TLS user config (threading) |
| `src/core/` | C++ core: `mbedtls_py::core` target, threading registration, digest/HMAC helpers |
| `src/mbedtls/` | Python package and the Cython module `_mbedtls.pyx` |
| `examples/consumer` | smallest user of the installed package (also run in CI) |
| `tests/` | `pytest` for Python, `tests/cpp/smoke.cpp` for the core (ctest) |
