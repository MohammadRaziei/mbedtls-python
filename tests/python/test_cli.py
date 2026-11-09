import subprocess
import sys

import mbedtls


def run(*args):
    done = subprocess.run([sys.executable, "-m", "mbedtls", *args], capture_output=True, text=True, check=True)
    return done.stdout


def test_version_prints_the_package_version_alone():
    # In a staged install without wheel metadata __version__ is the fallback, so compare with it, not with a number.
    assert run("--version") == mbedtls.__version__ + "\n"
    assert run("-v") == mbedtls.__version__ + "\n"


def test_mbedtls_version_prints_the_library_version_alone():
    out = run("--mbedtls-version")
    assert out == mbedtls.__mbedtls_version__ + "\n"
    major, minor, patch = (int(part) for part in out.split("."))
    assert major >= 4


def test_without_flags_both_versions_are_shown():
    out = run()
    assert mbedtls.__version__ in out and mbedtls.__mbedtls_version__ in out


def test_path_flags_still_print_one_path():
    assert run("--cmake-dir").strip() == mbedtls.get_cmake_dir()
    assert run("--sdk-dir").strip() == mbedtls.get_sdk_dir()
