import base64
import datetime
import hashlib
import threading
from pathlib import Path

import pytest

from mbedtls import x509
from mbedtls.x509 import Certificate, VerificationError, load_pem_bundle

FIXTURES = Path(__file__).parent / "fixtures"


def pem(name):
    return (FIXTURES / f"{name}.pem").read_text()


@pytest.fixture(scope="module")
def certs():
    return {n: Certificate.from_pem(pem(n)) for n in ("root", "int", "leaf", "expired", "other_root", "rsa_root", "rsa_leaf")}


def test_fields(certs):
    leaf = certs["leaf"]
    assert leaf.version == 3
    assert "CN=leaf.example.test" in leaf.subject
    assert "CN=Test Intermediate CA" in leaf.issuer
    assert leaf.serial_number == 0x1234567890ABCDEF
    assert leaf.not_before == datetime.datetime(2020, 1, 1, tzinfo=datetime.timezone.utc)
    assert leaf.not_after == datetime.datetime(2126, 1, 1, tzinfo=datetime.timezone.utc)
    assert leaf.not_before.utcoffset() == datetime.timedelta(0)
    assert not leaf.is_ca
    assert [certs[n].is_ca for n in ("root", "int", "other_root", "rsa_root")] == [True] * 4
    assert certs["root"].subject == certs["root"].issuer  # self-signed
    assert certs["expired"].serial_number == 0x0A
    assert certs["rsa_leaf"].serial_number == 5


def test_der_and_pem_round_trip(certs):
    leaf = certs["leaf"]
    assert Certificate.from_der(leaf.der) == leaf
    assert Certificate.from_pem(leaf.to_pem()) == leaf
    assert Certificate.from_pem(leaf.to_pem().encode()) == leaf
    assert Certificate.from_pem(bytearray(pem("leaf").encode())) == leaf
    assert leaf.to_pem().strip() == pem("leaf").strip()
    body = "".join(pem("leaf").splitlines()[1:-1])
    assert base64.b64decode(body) == leaf.der


def test_fingerprint(certs):
    leaf = certs["leaf"]
    assert leaf.fingerprint() == hashlib.sha256(leaf.der).digest()
    assert leaf.fingerprint("sha1") == hashlib.sha1(leaf.der).digest()
    with pytest.raises(ValueError):
        leaf.fingerprint("nope")


def test_equality_hash_repr_immutable(certs):
    a, b = Certificate.from_pem(pem("leaf")), Certificate.from_pem(pem("leaf"))
    assert a == b and hash(a) == hash(b) and a != certs["int"] and a != "x"
    assert len({a, b, certs["int"]}) == 2
    assert "leaf.example.test" in repr(a)
    with pytest.raises(AttributeError):
        a.subject = "x"
    with pytest.raises(AttributeError):
        del a.der


def test_bundle():
    bundle = pem("root") + pem("int") + pem("leaf")
    certs = load_pem_bundle(bundle)
    assert [c.serial_number for c in certs] == [1, 2, 0x1234567890ABCDEF]
    assert load_pem_bundle(bundle.encode()) == certs
    with pytest.raises(ValueError, match="exactly one"):
        Certificate.from_pem(bundle)
    assert load_pem_bundle(pem("root") + "\nsome text between blocks\n" + pem("int"))[1].serial_number == 2


@pytest.mark.parametrize("bad", ["", "garbage", "-----BEGIN CERTIFICATE-----\nAAAA\n-----END CERTIFICATE-----\n"])
def test_bad_pem(bad):
    with pytest.raises(ValueError):
        Certificate.from_pem(bad)


def test_bad_der(certs):
    der = certs["leaf"].der
    for bad in (b"", b"\x30\x03abc", der[:-5], der[: len(der) // 2], der + b"junk", b"\xff" * 64):
        with pytest.raises(ValueError):
            Certificate.from_der(bad)


def test_bad_types():
    with pytest.raises(TypeError):
        Certificate.from_der("text")
    with pytest.raises(TypeError):
        Certificate.from_pem(5)
    with pytest.raises(ValueError):
        Certificate.from_pem(pem("leaf") + "\x00")
    with pytest.raises(ValueError):
        Certificate.from_pem("caf\u00e9")


def test_verify_ok(certs):
    leaf, inter, root = certs["leaf"], certs["int"], certs["root"]
    assert leaf.verify(root, intermediates=[inter]) is None
    assert leaf.verify([root], intermediates=[inter], hostname="leaf.example.test") is None
    assert leaf.verify([certs["other_root"], root], intermediates=(inter,), hostname="a.wild.example.test") is None
    assert inter.verify(root) is None
    assert root.verify(root) is None  # a self-signed anchor trusts itself
    assert certs["rsa_leaf"].verify(certs["rsa_root"], hostname="leaf.example.test") is None  # RSA 2048


def test_verify_failures(certs):
    leaf, inter, root = certs["leaf"], certs["int"], certs["root"]

    def reasons(*args, **kw):
        with pytest.raises(VerificationError) as e:
            leaf.verify(*args, **kw)
        assert isinstance(e.value, ValueError) and e.value.flags != 0 and e.value.reasons
        return " ".join(e.value.reasons).lower()

    assert "not" in reasons(root)  # the intermediate is missing
    assert "trusted" in reasons(certs["other_root"], intermediates=[inter])
    assert "trusted" in reasons([], intermediates=[inter])
    with pytest.raises(VerificationError) as e:
        leaf.verify(root, intermediates=[inter], hostname="wrong.example.test")
    assert e.value.flags != 0
    with pytest.raises(VerificationError) as e:
        certs["expired"].verify(root, intermediates=[inter])
    assert "expired" in " ".join(e.value.reasons).lower()
    with pytest.raises(VerificationError):  # the RSA leaf under an EC root
        certs["rsa_leaf"].verify(root)


def test_verify_bad_arguments(certs):
    leaf, root = certs["leaf"], certs["root"]
    with pytest.raises(TypeError):
        leaf.verify(["not a cert"])
    with pytest.raises(TypeError):
        leaf.verify(root, intermediates=[b"der"])
    with pytest.raises(TypeError):
        leaf.verify(root, hostname=b"leaf.example.test")
    for bad in ("", "a\x00b", "caf\u00e9.example"):
        with pytest.raises(ValueError):
            leaf.verify(root, hostname=bad)


def test_verify_threads(certs):
    leaf, inter, root = certs["leaf"], certs["int"], certs["root"]
    results = []

    def work():
        for _ in range(40):
            leaf.verify(root, intermediates=[inter], hostname="leaf.example.test")
            try:
                leaf.verify(root, hostname="leaf.example.test")  # no intermediate
            except VerificationError:
                results.append("fail")
        results.append("done")

    threads = [threading.Thread(target=work) for _ in range(8)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert results.count("done") == 8 and results.count("fail") == 8 * 40


def test_module_exports():
    import mbedtls

    assert mbedtls.x509 is x509
    assert set(x509.__all__) == {"Certificate", "VerificationError", "load_pem_bundle"}
