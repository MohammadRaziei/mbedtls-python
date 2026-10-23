// ponytail: is_ca is read from a field Mbed TLS 4.2 marks private (ca_istrue) because it has no accessor for it.
// Upgrade path: drop this define the day an accessor exists; a field rename shows up as a compile error here.
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "mbedtls_cpp_x509.h"

#include <mbedtls/error.h>
#include <mbedtls/x509_crt.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

struct mbedtls_cpp_x509 {
    mbedtls_x509_crt crt;
};

namespace {

// Frees what a stack frame parsed, whatever return path it takes.
struct Chain {
    mbedtls_x509_crt crt;
    Chain() { mbedtls_x509_crt_init(&crt); }
    ~Chain() { mbedtls_x509_crt_free(&crt); }
    Chain(const Chain&) = delete;
    Chain& operator=(const Chain&) = delete;
};

int parse_all(Chain& chain, const unsigned char* const* ders, const size_t* lens, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const int ret = mbedtls_x509_crt_parse_der(&chain.crt, ders[i], lens[i]);
        if (ret != 0) return ret;
    }
    return 0;
}

const mbedtls_x509_crt* nth(const mbedtls_x509_crt* crt, size_t i) {
    for (; crt != nullptr && crt->raw.p != nullptr; crt = crt->next, --i)
        if (i == 0) return crt;
    return nullptr;
}

} // namespace

extern "C" int mbedtls_cpp_x509_parse(const unsigned char* buf, size_t len, int pem, mbedtls_cpp_x509** out) {
    *out = nullptr;
    auto* h = new (std::nothrow) mbedtls_cpp_x509;
    if (h == nullptr) return MBEDTLS_ERR_X509_ALLOC_FAILED;
    mbedtls_x509_crt_init(&h->crt);

    int ret;
    if (pem) {
        // mbedtls_x509_crt_parse() takes text only when it is NUL terminated, and the NUL counts in the length.
        auto* text = static_cast<unsigned char*>(std::malloc(len + 1));
        if (text == nullptr) {
            delete h;
            return MBEDTLS_ERR_X509_ALLOC_FAILED;
        }
        std::memcpy(text, buf, len);
        text[len] = 0;
        ret = mbedtls_x509_crt_parse(&h->crt, text, len + 1);
        std::free(text);
    } else {
        ret = mbedtls_x509_crt_parse_der(&h->crt, buf, len);
        // Mbed TLS reads the length from the DER header and ignores whatever follows it: refuse that here.
        if (ret == 0 && h->crt.raw.len != len) ret = MBEDTLS_ERR_X509_INVALID_FORMAT;
    }
    if (ret != 0) {
        mbedtls_x509_crt_free(&h->crt);
        delete h;
        return ret;
    }
    *out = h;
    return 0;
}

extern "C" void mbedtls_cpp_x509_free(mbedtls_cpp_x509* chain) {
    if (chain == nullptr) return;
    mbedtls_x509_crt_free(&chain->crt);
    delete chain;
}

extern "C" size_t mbedtls_cpp_x509_count(const mbedtls_cpp_x509* chain) {
    size_t n = 0;
    while (nth(&chain->crt, n) != nullptr) ++n;
    return n;
}

extern "C" int mbedtls_cpp_x509_get_info(const mbedtls_cpp_x509* chain, size_t i, mbedtls_cpp_x509_info* out) {
    const mbedtls_x509_crt* c = nth(&chain->crt, i);
    if (c == nullptr) return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    if (c->serial.len > MBEDTLS_CPP_X509_SERIAL_MAX) return MBEDTLS_ERR_X509_BUFFER_TOO_SMALL;

    int ret = mbedtls_x509_dn_gets(out->subject, sizeof out->subject, &c->subject);
    if (ret < 0) return ret;
    ret = mbedtls_x509_dn_gets(out->issuer, sizeof out->issuer, &c->issuer);
    if (ret < 0) return ret;

    out->der = c->raw.p;
    out->der_len = c->raw.len;
    out->version = c->version;
    out->is_ca = c->MBEDTLS_PRIVATE(ca_istrue) != 0;
    out->serial_len = c->serial.len;
    std::memcpy(out->serial, c->serial.p, c->serial.len);
    const mbedtls_x509_time* t[2] = {&c->valid_from, &c->valid_to};
    int* dst[2] = {out->not_before, out->not_after};
    for (int k = 0; k < 2; ++k) {
        dst[k][0] = t[k]->year;
        dst[k][1] = t[k]->mon;
        dst[k][2] = t[k]->day;
        dst[k][3] = t[k]->hour;
        dst[k][4] = t[k]->min;
        dst[k][5] = t[k]->sec;
    }
    return 0;
}

extern "C" int mbedtls_cpp_x509_verify(const unsigned char* const* chain, const size_t* chain_len, size_t chain_n,
                                      const unsigned char* const* trust, const size_t* trust_len, size_t trust_n,
                                      const char* hostname, uint32_t* flags, char* info, size_t info_size) {
    Chain leaf, anchors;
    int ret = parse_all(leaf, chain, chain_len, chain_n);
    if (ret != 0) return ret;
    ret = parse_all(anchors, trust, trust_len, trust_n);
    if (ret != 0) return ret;

    uint32_t f = 0;
    // No anchors at all: pass NULL, Mbed TLS then reports the chain as not trusted.
    ret = mbedtls_x509_crt_verify(&leaf.crt, trust_n ? &anchors.crt : nullptr, nullptr, hostname, &f, nullptr,
                                  nullptr);
    *flags = f;
    if (ret == 0) return 0;
    if (ret != MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) return ret;
    if (info != nullptr && info_size > 0) {
        info[0] = '\0';
        mbedtls_x509_crt_verify_info(info, info_size, "", f);
    }
    return MBEDTLS_CPP_X509_VERIFY_FAILED;
}

extern "C" void mbedtls_cpp_x509_strerror(int code, char* buf, size_t size) {
    if (code > 0) {
        std::snprintf(buf, size, "%d certificate(s) of the bundle could not be parsed", code);
        return;
    }
    mbedtls_strerror(code, buf, size);
}
