
`pip install mbedtls==VERSION_STRING`

| Asset | What it is |
| :--- | :--- |
| `mbedtls-VERSION_STRING-cp39-abi3-*.whl` | one wheel per platform for every GIL-enabled CPython from 3.9 (stable ABI) |
| `mbedtls-VERSION_STRING-cp313t / cp314t / cp315t-*.whl` | free-threaded CPython (abi3 cannot be loaded there) |
| `mbedtls-VERSION_STRING.tar.gz` | sdist, carries the Mbed TLS sources |
| `mbedtls-cpp-core-VERSION_STRING-<OS>-<arch>.*` | prebuilt C++ core for `find_package(mbedtls_cpp CONFIG)`, see the [README](https://github.com/MohammadRaziei/mbedtls-python#use-the-c-core-without-building-mbed-tls) |

Source: [zip](https://github.com/MohammadRaziei/mbedtls-python/archive/refs/tags/RELEASE_NAME.zip), [tar.gz](https://github.com/MohammadRaziei/mbedtls-python/archive/refs/tags/RELEASE_NAME.tar.gz) (these do not contain the Mbed TLS submodule: use the sdist, or clone with `--recurse-submodules`).
