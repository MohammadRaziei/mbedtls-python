from pathlib import Path

import mbedtls


def test_package_dirs_exist():
    assert (Path(mbedtls.get_cmake_dir()) / "mbedtls_py-config.cmake").is_file()
    assert (Path(mbedtls.get_include_dir()) / "mbedtls_py_crypto.h").is_file()
    assert (Path(mbedtls.get_include_dir()) / "psa" / "crypto.h").is_file()
    assert any(Path(mbedtls.get_lib_dir()).glob("*mbedtls_py_core*"))
