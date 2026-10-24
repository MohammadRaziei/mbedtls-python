from pathlib import Path

import mbedtls


def test_package_dirs_exist():
    assert (Path(mbedtls.get_cmake_dir()) / "MbedTLSConfig.cmake").is_file()
    assert (Path(mbedtls.get_include_dir()) / "mbedtls_cpp" / "crypto.hpp").is_file()
    assert (Path(mbedtls.get_include_dir()) / "psa" / "crypto.h").is_file()
    assert any(Path(mbedtls.get_lib_dir()).glob("*mbedtls_cpp_core*"))
