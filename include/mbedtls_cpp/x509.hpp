#pragma once
// X.509 certificates: parse, inspect, verify a chain. A Certificate is an immutable value (the DER plus the fields
// read from it) and owns no native resource, so it can be copied and shared between threads freely.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "error.hpp"

namespace mbedtls_cpp {

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

}  // namespace mbedtls_cpp
