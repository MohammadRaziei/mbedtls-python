"""X.509 certificates: parse (PEM or DER), inspect, and verify a chain against trust anchors.

Certificates are immutable and safe to share between threads. Verification uses Mbed TLS's default profile (SHA-2
signatures, RSA of at least 2048 bits, the common curves) and the current time; there is no CRL or OCSP support.
"""

import base64
import datetime

from . import _mbedtls

__all__ = ["Certificate", "VerificationError", "load_pem_bundle"]

_UTC = datetime.timezone.utc


class VerificationError(ValueError):
    """The chain did not verify. ``reasons`` lists Mbed TLS's findings, ``flags`` is its raw MBEDTLS_X509_BADCERT_*
    bitmask."""

    def __init__(self, reasons, flags):
        super().__init__("; ".join(reasons) or f"certificate verification failed (flags 0x{flags:x})")
        self.reasons = list(reasons)
        self.flags = flags


def _to_bytes(data, what, *, text_ok=False):
    if isinstance(data, bytes):
        return data
    if isinstance(data, (bytearray, memoryview)):
        return bytes(data)
    if text_ok and isinstance(data, str):
        try:
            return data.encode("ascii")
        except UnicodeEncodeError:
            raise ValueError(f"{what} must be ASCII text") from None
    raise TypeError(f"{what} must be bytes-like{' or str' if text_ok else ''}, not {type(data).__name__}")


def _time(parts):
    return datetime.datetime(*parts, tzinfo=_UTC)


class Certificate:
    """One X.509 certificate. Build it with :meth:`from_pem` or :meth:`from_der`.

    Attributes: ``der`` (bytes), ``version`` (1-3), ``subject`` and ``issuer`` (str, Mbed TLS's ``"CN=x, O=y"``
    formatting), ``serial_number`` (int), ``not_before`` / ``not_after`` (timezone-aware UTC datetimes) and ``is_ca``
    (basicConstraints).
    """

    __slots__ = ("der", "version", "subject", "issuer", "serial_number", "not_before", "not_after", "is_ca")

    def __init__(self, der):
        """``der``: one binary DER certificate (bytes-like)."""
        entries = _mbedtls._x509_parse(_to_bytes(der, "der"), False)
        self._set(entries[0])

    def _set(self, entry):
        der, version, is_ca, subject, issuer, serial, not_before, not_after = entry
        for name, value in (
            ("der", der),
            ("version", version),
            ("is_ca", is_ca),
            ("subject", subject),
            ("issuer", issuer),
            ("serial_number", int.from_bytes(serial, "big")),
            ("not_before", _time(not_before)),
            ("not_after", _time(not_after)),
        ):
            object.__setattr__(self, name, value)

    def __setattr__(self, name, value):
        raise AttributeError("Certificate is immutable")

    def __delattr__(self, name):
        raise AttributeError("Certificate is immutable")

    @classmethod
    def _from_entry(cls, entry):
        cert = object.__new__(cls)
        cert._set(entry)
        return cert

    @classmethod
    def from_der(cls, data):
        """Parse one binary DER certificate (``ValueError`` if it is malformed or has trailing data)."""
        return cls(data)

    @classmethod
    def from_pem(cls, data):
        """Parse exactly one PEM certificate (``str`` or bytes-like). For a bundle use :func:`load_pem_bundle`."""
        entries = _load_pem(data)
        if len(entries) != 1:
            raise ValueError(f"expected exactly one certificate, found {len(entries)} (see load_pem_bundle)")
        return cls._from_entry(entries[0])

    def to_pem(self):
        """The certificate as PEM text."""
        b64 = base64.b64encode(self.der).decode("ascii")
        body = "\n".join(b64[i : i + 64] for i in range(0, len(b64), 64))
        return f"-----BEGIN CERTIFICATE-----\n{body}\n-----END CERTIFICATE-----\n"

    def fingerprint(self, algorithm="sha256"):
        """Digest of the DER encoding (bytes); ``algorithm`` is one of :data:`mbedtls.algorithms`."""
        return _mbedtls.digest(algorithm, self.der)

    def verify(self, trusted, *, intermediates=(), hostname=None):
        """Check that this certificate chains up to one of ``trusted`` (a Certificate or an iterable of them),
        through ``intermediates`` (any order), and, if ``hostname`` is given, that it is valid for that name (a
        subjectAltName, or the common name when there is no SAN; internationalized names in punycode).

        Returns ``None`` when it verifies and raises :class:`VerificationError` when it does not.
        """
        if isinstance(trusted, Certificate):
            trusted = [trusted]
        chain = [self.der] + [_der_of(c) for c in intermediates]
        anchors = [_der_of(c) for c in trusted]
        host = None
        if hostname is not None:
            if not isinstance(hostname, str):
                raise TypeError(f"hostname must be str, not {type(hostname).__name__}")
            if not hostname or "\x00" in hostname:
                raise ValueError("hostname must be a non-empty string without NUL")
            try:
                host = hostname.encode("ascii")
            except UnicodeEncodeError:
                raise ValueError("hostname must be ASCII: encode internationalized names to punycode") from None
        status, flags, text = _mbedtls._x509_verify(chain, anchors, host)
        if status != 0:
            raise VerificationError([line.strip() for line in text.splitlines() if line.strip()], flags)

    def __eq__(self, other):
        return isinstance(other, Certificate) and self.der == other.der

    def __hash__(self):
        return hash(self.der)

    def __repr__(self):
        return f"<Certificate subject={self.subject!r} serial={self.serial_number:#x}>"


def _der_of(cert):
    if not isinstance(cert, Certificate):
        raise TypeError(f"expected Certificate, not {type(cert).__name__}")
    return cert.der


def _load_pem(data):
    pem = _to_bytes(data, "data", text_ok=True)
    if b"\x00" in pem:
        raise ValueError("PEM data must not contain NUL")
    return _mbedtls._x509_parse(pem, True)


def load_pem_bundle(data):
    """Parse every certificate of a PEM bundle (``str`` or bytes-like) into a list, in file order. Fails if any
    block cannot be parsed."""
    return [Certificate._from_entry(e) for e in _load_pem(data)]
