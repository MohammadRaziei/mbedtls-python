#pragma once
// C++ layer over Mbed TLS: hashes, HMAC, random bytes and X.509 certificates, as small classes. Data goes in as
// std::string_view and comes out as std::string: both are plain byte containers here, embedded NULs included.
// Errors are exceptions, derived from std types so that Cython maps them to ValueError / RuntimeError by itself.
// Implementation: src/mbedtls.cpp.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace mbedtls {

// ---- version ----

// The version of Mbed TLS itself (what this library was built against), not of this project: that one is the
// `version` of pyproject.toml.
std::string version_string();  // "4.2.0"
uint32_t version_hex();        // 0x04020000: major, minor, patch, one byte each (MBEDTLS_VERSION_NUMBER)

// ---- errors ----
// Mbed TLS / PSA reported a failure. code() is the (negative) psa_status_t or Mbed TLS error code.
class Error : public std::runtime_error {
public:
    Error(const std::string& what, int code) : std::runtime_error(what), code_(code) {}
    int code() const noexcept { return code_; }

private:
    int code_;
};

// The caller named a hash algorithm that is not supported (see hash_algorithms()).
class UnknownAlgorithm : public std::invalid_argument {
public:
    explicit UnknownAlgorithm(std::string_view name)
        : std::invalid_argument("unsupported hash algorithm: '" + std::string(name) + "'") {}
};

// Malformed or unusable certificate data. code() is the Mbed TLS error code, or the number of certificates of a PEM
// bundle that could not be parsed (positive).
class CertificateError : public std::invalid_argument {
public:
    CertificateError(const std::string& what, int code) : std::invalid_argument("invalid certificate: " + what), code_(code) {}
    int code() const noexcept { return code_; }

private:
    int code_;
};

// ---- init ----
// Registers the alt threading layer when the build selected it (Windows, or MBEDTLS_CPP_THREADING_ALT; on POSIX the
// pthread layer needs nothing) and runs psa_crypto_init(). Safe to call more than once and from several threads. The
// classes and functions of this library call it themselves on first use; call it explicitly before using the Mbed TLS
// / PSA C API directly. Throws Error when psa_crypto_init() fails.
void init();

// ---- crypto ----
// Names of the supported hash algorithms, spelled like Python's hashlib ("sha256", "sha3_512", ...).
std::vector<std::string> hash_algorithms();

// A hash algorithm chosen by name. Throws UnknownAlgorithm when there is no such algorithm.
class Digest {
public:
    explicit Digest(std::string_view name);

    const std::string& name() const noexcept { return name_; }
    size_t size() const noexcept;  // digest length in bytes
    std::string compute(std::string_view data) const;

private:
    std::string name_;
    uint32_t alg_;  // psa_algorithm_t, kept out of this header
};

// HMAC under a fixed key (may be empty). The key is copied and wiped (best effort) in the destructor.
class Hmac {
public:
    Hmac(std::string_view name, std::string_view key);
    Hmac(const Hmac&) = default;
    Hmac(Hmac&&) = default;
    Hmac& operator=(const Hmac&) = default;
    Hmac& operator=(Hmac&&) = default;
    ~Hmac();

    const std::string& name() const noexcept { return digest_.name(); }
    std::string compute(std::string_view data) const;

private:
    Digest digest_;
    std::string key_;
};

// One-shot helpers.
std::string digest(std::string_view name, std::string_view data);
std::string hmac(std::string_view name, std::string_view key, std::string_view data);
std::string random_bytes(size_t n);  // from the PSA random generator

// ---- x509 ----
struct Time {  // UTC
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
};

// Outcome of Certificate::verify(). When ok is false, `flags` holds Mbed TLS's MBEDTLS_X509_BADCERT_* bits and `info`
// its description of the problems, one per line.
struct VerifyResult {
    bool ok = false;
    uint32_t flags = 0;
    std::string info;
};

class Certificate {
public:
    // An empty placeholder (not a valid certificate); get real ones from from_der() / from_pem().
    Certificate() = default;

    // One binary DER certificate. Bytes after it are refused (Mbed TLS would ignore them). Throws CertificateError.
    static Certificate from_der(std::string_view der);
    // Every "BEGIN CERTIFICATE" block of a PEM text, in order. Throws CertificateError if any does not parse.
    static std::vector<Certificate> from_pem(std::string_view pem);

    const std::string& der() const noexcept { return der_; }
    int version() const noexcept { return version_; }  // 1, 2 or 3
    bool is_ca() const noexcept { return is_ca_; }     // basicConstraints cA
    const std::string& subject() const noexcept { return subject_; }  // Mbed TLS's "CN=x, O=y" formatting
    const std::string& issuer() const noexcept { return issuer_; }
    const std::string& serial() const noexcept { return serial_; }  // big endian
    const Time& not_before() const noexcept { return not_before_; }
    const Time& not_after() const noexcept { return not_after_; }

    // Verifies this certificate, with `intermediates` (any order), against the `trusted` anchors. The hostname, when
    // given, must match a subjectAltName (or the common name when there is none). Uses Mbed TLS's default profile
    // (SHA-2, RSA >= 2048 bits, the common curves). A chain that does not verify is a result, not an exception.
    VerifyResult verify(const std::vector<Certificate>& trusted, const std::vector<Certificate>& intermediates = {}) const;
    VerifyResult verify(const std::vector<Certificate>& trusted, const std::vector<Certificate>& intermediates,
                        std::string_view hostname) const;

    bool operator==(const Certificate& o) const noexcept { return der_ == o.der_; }
    bool operator!=(const Certificate& o) const noexcept { return der_ != o.der_; }

private:
    static Certificate from_crt(const void* mbedtls_x509_crt_node);  // reads the fields off one parsed node
    VerifyResult verify_impl(const std::vector<Certificate>& trusted, const std::vector<Certificate>& intermediates,
                             const std::string* hostname) const;

    std::string der_, subject_, issuer_, serial_;
    int version_ = 0;
    bool is_ca_ = false;
    Time not_before_, not_after_;
};

}  // namespace mbedtls
