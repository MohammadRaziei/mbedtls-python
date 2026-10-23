import argparse

from . import __version__, get_cmake_dir, get_include_dir, get_lib_dir, version


def cli():
    # Build-system helpers (same idea as `python -m nanobind --cmake_dir`): print a path, so a CMakeLists.txt can do
    # execute_process(python -m mbedtls --cmake-dir) and hand the result to find_package(mbedtls_py).
    p = argparse.ArgumentParser(prog="mbedtls")
    p.add_argument("--cmake-dir", "--cmake_dir", action="store_true", help="print the directory with mbedtls_py-config.cmake")
    p.add_argument("--include-dir", "--include_dir", action="store_true", help="print the directory with the headers")
    p.add_argument("--lib-dir", "--lib_dir", action="store_true", help="print the directory with the static libraries")
    a = p.parse_args()
    for flag, getter in ((a.cmake_dir, get_cmake_dir), (a.include_dir, get_include_dir), (a.lib_dir, get_lib_dir)):
        if flag:
            print(getter())
            return
    print(f"mbedtls {__version__} (Mbed TLS {version()})")


if __name__ == "__main__":
    cli()
