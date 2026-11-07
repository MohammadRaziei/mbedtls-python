from pathlib import Path

import mbedtls


def test_sdk_layout():
    sdk = Path(mbedtls.get_sdk_dir())
    assert sdk.name == "sdk" and sdk.parent == Path(mbedtls.__file__).parent
    assert Path(mbedtls.get_include_dir()) == sdk / "include"
    assert Path(mbedtls.get_lib_dir()) == sdk / "lib"
    assert Path(mbedtls.get_cmake_dir()) == sdk / "lib" / "cmake" / "mbedtls_cpp"


def test_package_dirs_exist():
    assert (Path(mbedtls.get_cmake_dir()) / "mbedtls_cppConfig.cmake").is_file()
    assert (Path(mbedtls.get_cmake_dir()) / "mbedtls_cppTargets.cmake").is_file()
    assert (Path(mbedtls.get_include_dir()) / "mbedtls.hpp").is_file()
    assert (Path(mbedtls.get_include_dir()) / "mbedtls_cpp" / "export.hpp").is_file()
    # a shared library: libmbedtls_cpp.so / libmbedtls_cpp.dylib / mbedtls_cpp.dll
    assert any(p.suffix in (".so", ".dylib", ".dll") or ".so." in p.name for p in Path(mbedtls.get_lib_dir()).glob("*mbedtls_cpp*"))
