import hashlib
import re
import threading
from pathlib import Path

import pytest

import mbedtls


def test_versions():
    assert re.fullmatch(r"\d+\.\d+\.\d+", mbedtls.__version__)
    assert re.fullmatch(r"\d+\.\d+\.\d+", mbedtls.version())


@pytest.mark.parametrize("data", [b"", b"abc", b"x" * 1000, bytes(range(256)) * 17])
def test_sha256_matches_hashlib(data):
    assert mbedtls.sha256(data) == hashlib.sha256(data).digest()


def test_sha256_bytes_like():
    assert mbedtls.sha256(bytearray(b"abc")) == hashlib.sha256(b"abc").digest()
    assert mbedtls.sha256(memoryview(b"abc")) == hashlib.sha256(b"abc").digest()


@pytest.mark.parametrize("bad", ["abc", 5, None, [1, 2], 1.5])
def test_sha256_rejects_non_bytes(bad):
    with pytest.raises(TypeError):
        mbedtls.sha256(bad)


def test_sha256_large():
    data = b"\xa5" * (32 * 1024 * 1024)
    assert mbedtls.sha256(data) == hashlib.sha256(data).digest()


def test_sha256_threads():
    # Exercises the threading layer (PSA global state is locked) and the GIL release.
    data = [bytes([i]) * 4096 for i in range(16)]
    errors = []

    def work():
        for _ in range(100):
            for d in data:
                if mbedtls.sha256(d) != hashlib.sha256(d).digest():
                    errors.append(d[:1])

    threads = [threading.Thread(target=work) for _ in range(8)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert not errors


def test_pyx_stays_abi3_safe():
    pyx = Path(__file__).parent.parent / "src" / "mbedtls" / "_mbedtls.pyx"
    if not pyx.exists():  # tests run against an installed wheel elsewhere
        pytest.skip("source tree not available")
    assert "[::" not in pyx.read_text(), "typed memoryviews are not in the limited API before Python 3.11"
