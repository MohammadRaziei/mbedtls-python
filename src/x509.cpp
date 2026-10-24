// ponytail: is_ca is read from a field Mbed TLS 4.2 marks private (ca_istrue) because it has no accessor for it.
// Upgrade path: drop this define the day an accessor exists; a field rename shows up as a compile error here.
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "mbedtls_cpp/x509.hpp"

#include <mbedtls/error.h>
#include <mbedtls/x509_crt.h>

#include <cstdio>

#include "mbedtls_cpp/init.hpp"

namespace mbedtls_cpp {
namespace {

// A parsed certificate list, freed on every return path.
class Crt {
public:
    Crt() { mbedtls_x509_crt_init(&crt_); }
    ~Crt() { mbedtls_x509_crt_free(&crt_); }
    Crt(const Crt&) = delete;
    Crt& operator=(const Crt&) = delete;
    mbedtls_x509_crt* get() { return &crt_; }

private:
    mbedtls_x509_crt crt_;
};

const unsigned char* bytes(std::string_view s) { return reinterpret_cast<const unsigned char*>(s.data()); }

[[noreturn]] void fail(int code) {
    if (code > 0)
        throw CertificateError(std::to_string(code) + " certificate(s) of the bundle could not be parsed", code);
    char text[256] = {0};
    mbedtls_strerror(code, text, sizeof text);
    char tail[16];
    std::snprintf(tail, sizeof tail, " [-0x%04X]", static_cast<unsigned>(-code));
    throw CertificateError(std::string(text) + tail, code);
}

void parse_der(Crt& crt, const Certificate& c) {
    const int ret = mbedtls_x509_crt_parse_der(crt.get(), bytes(c.der()), c.der().size());
    if (ret != 0) fail(ret);
}

std::string name_of(const mbedtls_x509_name* dn) {
    char buf[1024];
    const int ret = mbedtls_x509_dn_gets(buf, sizeof buf, dn);
    if (ret < 0) fail(ret);
    return std::string(buf, static_cast<size_t>(ret));
}

Time time_of(const mbedtls_x509_time& t) { return {t.year, t.mon, t.day, t.hour, t.min, t.sec}; }

}  // namespace

Certificate Certificate::from_crt(const void* node) {
    const auto* c = static_cast<const mbedtls_x509_crt*>(node);
    Certificate out;
    out.der_.assign(reinterpret_cast<const char*>(c->raw.p), c->raw.len);
    out.version_ = c->version;
    out.is_ca_ = c->MBEDTLS_PRIVATE(ca_istrue) != 0;
    out.subject_ = name_of(&c->subject);
    out.issuer_ = name_of(&c->issuer);
    out.serial_.assign(reinterpret_cast<const char*>(c->serial.p), c->serial.len);
    out.not_before_ = time_of(c->valid_from);
    out.not_after_ = time_of(c->valid_to);
    return out;
}

Certificate Certificate::from_der(std::string_view der) {
    init();
    Crt crt;
    int ret = mbedtls_x509_crt_parse_der(crt.get(), bytes(der), der.size());
    // Mbed TLS reads the length from the DER header and ignores whatever follows it: refuse that here.
    if (ret == 0 && crt.get()->raw.len != der.size()) ret = MBEDTLS_ERR_X509_INVALID_FORMAT;
    if (ret != 0) fail(ret);
    return from_crt(crt.get());
}

std::vector<Certificate> Certificate::from_pem(std::string_view pem) {
    init();
    // mbedtls_x509_crt_parse() takes text only when it is NUL terminated, and the NUL counts in the length.
    const std::string text(pem);
    Crt crt;
    const int ret = mbedtls_x509_crt_parse(crt.get(), bytes(text), text.size() + 1);
    if (ret != 0) fail(ret);
    std::vector<Certificate> out;
    for (const mbedtls_x509_crt* c = crt.get(); c != nullptr && c->raw.p != nullptr; c = c->next)
        out.push_back(from_crt(c));
    return out;
}

VerifyResult Certificate::verify(const std::vector<Certificate>& trusted,
                                 const std::vector<Certificate>& intermediates) const {
    return verify_impl(trusted, intermediates, nullptr);
}

VerifyResult Certificate::verify(const std::vector<Certificate>& trusted, const std::vector<Certificate>& intermediates,
                                 std::string_view hostname) const {
    // The C API takes a NUL-terminated string and would silently cut "good.example\0.evil" short.
    if (hostname.find('\0') != std::string_view::npos) throw std::invalid_argument("hostname contains a NUL byte");
    const std::string host(hostname);
    return verify_impl(trusted, intermediates, &host);
}

VerifyResult Certificate::verify_impl(const std::vector<Certificate>& trusted,
                                      const std::vector<Certificate>& intermediates, const std::string* hostname) const {
    init();
    // Mbed TLS links the certificates it is given into one list, so it gets fresh copies parsed from the DER each
    // time: that is what keeps a Certificate immutable and shareable.
    Crt chain, anchors;
    parse_der(chain, *this);
    for (const Certificate& c : intermediates) parse_der(chain, c);
    for (const Certificate& c : trusted) parse_der(anchors, c);

    VerifyResult result;
    // No anchors at all: pass NULL, Mbed TLS then reports the chain as not trusted.
    const int ret = mbedtls_x509_crt_verify(chain.get(), trusted.empty() ? nullptr : anchors.get(), nullptr,
                                            hostname ? hostname->c_str() : nullptr, &result.flags, nullptr, nullptr);
    if (ret == 0) {
        result.ok = true;
        return result;
    }
    if (ret != MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) fail(ret);
    char info[1024] = {0};
    mbedtls_x509_crt_verify_info(info, sizeof info, "", result.flags);
    result.info = info;
    return result;
}

}  // namespace mbedtls_cpp
