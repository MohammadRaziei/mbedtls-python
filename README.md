# mbedtls-python

Python bindings for [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) (4.2.0), built the same way as
[httpp](https://github.com/MohammadRaziei/httpp): Cython, scikit-build-core, one `cp39-abi3` wheel per platform plus
separate free-threaded wheels (`cp313t`, `cp314t`, `cp315t`).

Mbed TLS is a git submodule, built from source and linked **statically** into the extension, with hidden visibility:
no system package, no DLL, and no symbol clash with another copy in the same process.

**Status: level 1** (build system, thread safety, CI). The API is only `version()` and `sha256()` for now.

```python
import mbedtls

mbedtls.version()         # "4.2.0"  (the Mbed TLS this wheel was built against)
mbedtls.sha256(b"abc")    # 32 bytes, GIL released while hashing
```

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

## Layout

| Path | What |
|---|---|
| `src/third_party/mbedtls` | submodule, pinned to `v4.2.0` |
| `cmake/mbedtls_config.h` | Mbed TLS user config (threading) |
| `src/core/` | C++ core: `mbedtls_py::core` target, threading registration |
| `src/mbedtls/` | Python package and the Cython module `_mbedtls.pyx` |
| `tests/` | `pytest` for Python, `tests/cpp/smoke.cpp` for the core (ctest) |
