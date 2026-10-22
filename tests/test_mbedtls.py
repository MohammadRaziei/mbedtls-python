import hashlib
import hmac as std_hmac
import re
import threading
from pathlib import Path

import pytest

import mbedtls

DATA = [b"", b"abc", b"x" * 1000, bytes(range(256)) * 17]


def test_versions():
    assert re.fullmatch(r"\d+\.\d+\.\d+", mbedtls.__version__)
    assert re.fullmatch(r"\d+\.\d+\.\d+", mbedtls.version())


def test_algorithms():
    assert isinstance(mbedtls.algorithms, tuple)
    assert {"md5", "sha1", "sha224", "sha256", "sha384", "sha512", "sha3_256"} <= set(mbedtls.algorithms)
    # same spelling as hashlib, so every name works there too
    assert set(mbedtls.algorithms) <= hashlib.algorithms_available


@pytest.mark.parametrize("name", mbedtls.algorithms)
@pytest.mark.parametrize("data", DATA)
def test_digest_matches_hashlib(name, data):
    assert mbedtls.digest(name, data) == hashlib.new(name, data).digest()


@pytest.mark.parametrize("data", DATA)
def test_sha256_matches_hashlib(data):
    assert mbedtls.sha256(data) == hashlib.sha256(data).digest()


@pytest.mark.parametrize("name", mbedtls.algorithms)
@pytest.mark.parametrize("key_len", [0, 1, 20, 64, 65, 136, 200, 4096])
def test_hmac_matches_stdlib(name, key_len):
    key = bytes(range(256)) * 16
    key = key[:key_len]
    data = b"The quick brown fox" * 3
    assert mbedtls.hmac(name, key, data) == std_hmac.new(key, data, name).digest()


def test_hmac_rfc4231_case1():
    mac = mbedtls.hmac("sha256", b"\x0b" * 20, b"Hi There")
    assert mac.hex() == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"


def test_bytes_like():
    want = hashlib.sha256(b"abc").digest()
    assert mbedtls.sha256(bytearray(b"abc")) == want
    assert mbedtls.sha256(memoryview(b"abc")) == want
    assert mbedtls.sha256(memoryview(b"xabcx")[1:4]) == want
    assert mbedtls.hmac("sha256", bytearray(b"k"), memoryview(b"d")) == std_hmac.new(b"k", b"d", "sha256").digest()

    class Sub(bytes):
        pass

    assert mbedtls.sha256(Sub(b"abc")) == want


@pytest.mark.parametrize("bad", ["abc", 5, None, [1, 2], 1.5])
def test_rejects_non_bytes(bad):
    with pytest.raises(TypeError):
        mbedtls.sha256(bad)
    with pytest.raises(TypeError):
        mbedtls.digest("sha256", bad)
    with pytest.raises(TypeError):
        mbedtls.hmac("sha256", bad, b"")
    with pytest.raises(TypeError):
        mbedtls.hmac("sha256", b"", bad)


@pytest.mark.parametrize("bad", ["", "SHA256", "sha-256", "nope", "sha256\u00e9", "sha256\x00"])
def test_unknown_algorithm(bad):
    with pytest.raises(ValueError):
        mbedtls.digest(bad, b"")
    with pytest.raises(ValueError):
        mbedtls.hmac(bad, b"", b"")


@pytest.mark.parametrize("bad", [b"sha256", 256, None])
def test_algorithm_name_must_be_str(bad):
    with pytest.raises(TypeError):
        mbedtls.digest(bad, b"")


def test_digest_large():
    data = b"\xa5" * (32 * 1024 * 1024)
    assert mbedtls.sha256(data) == hashlib.sha256(data).digest()


def test_random_bytes():
    assert mbedtls.random_bytes(0) == b""
    for n in (1, 16, 32, 1000, 1 << 20):
        r = mbedtls.random_bytes(n)
        assert isinstance(r, bytes) and len(r) == n
    assert mbedtls.random_bytes(32) != mbedtls.random_bytes(32)
    assert len(set(mbedtls.random_bytes(1 << 16))) > 200  # not stuck on a few values


@pytest.mark.parametrize("bad", [-1, "3", 3.0, None, True])
def test_random_bytes_rejects_bad_n(bad):
    with pytest.raises((TypeError, ValueError)):
        mbedtls.random_bytes(bad)


def test_random_bytes_overflow():
    with pytest.raises((OverflowError, MemoryError)):
        mbedtls.random_bytes(1 << 80)


def test_threads():
    # Exercises the threading layer (PSA global state is locked) and the GIL release. A broken lock around the
    # shared random generator shows up as duplicate "random" blocks.
    data = [bytes([i]) * 4096 for i in range(16)]
    errors = []
    randoms = []

    def work():
        mine = []
        for _ in range(50):
            for d in data:
                if mbedtls.sha256(d) != hashlib.sha256(d).digest():
                    errors.append(("sha256", d[:1]))
                if mbedtls.hmac("sha512", b"key", d) != std_hmac.new(b"key", d, "sha512").digest():
                    errors.append(("hmac", d[:1]))
            mine.append(mbedtls.random_bytes(16))
        randoms.extend(mine)

    threads = [threading.Thread(target=work) for _ in range(8)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert not errors
    assert len(randoms) == 8 * 50 and len(set(randoms)) == len(randoms)


def test_pyx_stays_abi3_safe():
    pyx = Path(__file__).parent.parent / "src" / "mbedtls" / "_mbedtls.pyx"
    if not pyx.exists():  # tests run against an installed wheel elsewhere
        pytest.skip("source tree not available")
    assert "[::" not in pyx.read_text(), "typed memoryviews are not in the limited API before Python 3.11"
