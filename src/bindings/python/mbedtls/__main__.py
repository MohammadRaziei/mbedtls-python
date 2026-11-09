import argparse

from . import __mbedtls_version__, __version__, get_cmake_dir, get_include_dir, get_lib_dir, get_sdk_dir


def cli():
    # Build-system helpers (same idea as `python -m nanobind --cmake_dir`): print a path, so a CMakeLists.txt can do
    # execute_process(python -m mbedtls --cmake-dir) and hand the result to find_package(MbedTLS).
    p = argparse.ArgumentParser(prog="mbedtls")
    p.add_argument("--sdk-dir", "--sdk_dir", action="store_true", help="print the root of the C++ SDK (include/, lib/)")
    p.add_argument("--cmake-dir", "--cmake_dir", action="store_true", help="print the directory with MbedTLSConfig.cmake")
    p.add_argument("--include-dir", "--include_dir", action="store_true", help="print the directory with the Mbed TLS headers")
    p.add_argument("--lib-dir", "--lib_dir", action="store_true", help="print the directory with the shared library")
    # Version queries for build systems (a bare number on one line, no program name): a dependency manager can compare it
    # with a version range without parsing text. -v/--version is the version of this Python package (the wheel), and
    # --mbedtls-version the version of the Mbed TLS library inside it: they are different numbers.
    p.add_argument("-v", "--version", action="store_true", help="print the version of this package (the wheel), alone")
    p.add_argument("--mbedtls-version", "--mbedtls_version", action="store_true",
                   help="print the version of the Mbed TLS library inside the package, alone")
    a = p.parse_args()
    for flag, getter in ((a.sdk_dir, get_sdk_dir), (a.cmake_dir, get_cmake_dir), (a.include_dir, get_include_dir),
                         (a.lib_dir, get_lib_dir), (a.version, lambda: __version__),
                         (a.mbedtls_version, lambda: __mbedtls_version__)):
        if flag:
            print(getter())
            return
    print(f"mbedtls {__version__} (Mbed TLS {__mbedtls_version__})")


if __name__ == "__main__":
    cli()
