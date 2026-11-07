
`pip install mbedtls==VERSION_STRING`

| Asset | What it is |
| :--- | :--- |
| `mbedtls-VERSION_STRING-cp39-abi3-*.whl` | one wheel per platform for every GIL-enabled CPython from 3.9 (stable ABI) |
| `mbedtls-VERSION_STRING-cp313t / cp314t / cp315t-*.whl` | free-threaded CPython (abi3 cannot be loaded there) |
| `mbedtls-VERSION_STRING.tar.gz` | sdist, carries the Mbed TLS sources |

Every wheel carries the C++ SDK as `mbedtls/sdk/` (`include/`, `lib/` with the shared library `mbedtls_cpp`, `lib/cmake/mbedtls_cpp/`): `python -m mbedtls --cmake-dir` points `find_package(mbedtls_cpp CONFIG)` at it, see the [README](https://github.com/MohammadRaziei/mbedtls-python#use-from-c-via-pip).

Changes: [CHANGELOG](https://github.com/MohammadRaziei/mbedtls-python/blob/RELEASE_NAME/CHANGELOG.md)

Source: [zip](https://github.com/MohammadRaziei/mbedtls-python/archive/refs/tags/RELEASE_NAME.zip), [tar.gz](https://github.com/MohammadRaziei/mbedtls-python/archive/refs/tags/RELEASE_NAME.tar.gz) (these do not contain the Mbed TLS submodule: use the sdist, or clone with `--recurse-submodules`).
