from pathlib import Path

import mbedtls


def test_sdk_layout():
    sdk = Path(mbedtls.get_sdk_dir())
    assert sdk.name == "sdk" and sdk.parent == Path(mbedtls.__file__).parent
    assert Path(mbedtls.get_include_dir()) == sdk / "include"
    assert Path(mbedtls.get_lib_dir()) == sdk / "lib"
    assert Path(mbedtls.get_cmake_dir()) == sdk / "lib" / "cmake" / "MbedTLS"


def test_package_dirs_exist():
    assert (Path(mbedtls.get_cmake_dir()) / "MbedTLSConfig.cmake").is_file()
    assert (Path(mbedtls.get_cmake_dir()) / "MbedTLSTargets.cmake").is_file()
    # Mbed TLS itself: its headers, and shared libraries named after its targets
    assert (Path(mbedtls.get_include_dir()) / "mbedtls" / "ssl.h").is_file()
    assert (Path(mbedtls.get_include_dir()) / "psa" / "crypto.h").is_file()
    for lib in ("mbedtls", "mbedx509", "tfpsacrypto"):
        found = list(Path(mbedtls.get_lib_dir()).glob(f"*{lib}*"))
        assert any(p.suffix in (".so", ".dylib", ".lib") or ".so." in p.name for p in found), lib
