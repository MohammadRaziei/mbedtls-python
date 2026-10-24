// The C++ layer: init (threading + PSA), hashes/HMAC/random, X.509. See include/mbedtls.hpp.
// ponytail: is_ca is read from a field Mbed TLS 4.2 marks private (ca_istrue), because it has no accessor for it, so
// the whole file is compiled with private access. Upgrade path: drop the define the day an accessor exists; a field
// rename shows up as a compile error.
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "mbedtls.hpp"

#include <mbedtls/build_info.h>
#include <mbedtls/error.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include <cstdio>
#include <mutex>

#if defined(MBEDTLS_THREADING_ALT)
#include <mbedtls/threading.h>

namespace {

#if defined(_WIN32)
int mutex_init(mbedtls_platform_mutex_t* m) { InitializeSRWLock(m); return 0; }
void mutex_destroy(mbedtls_platform_mutex_t*) {}
int mutex_lock(mbedtls_platform_mutex_t* m) { AcquireSRWLockExclusive(m); return 0; }
int mutex_unlock(mbedtls_platform_mutex_t* m) { ReleaseSRWLockExclusive(m); return 0; }
int cond_init(mbedtls_platform_condition_variable_t* c) { InitializeConditionVariable(c); return 0; }
void cond_destroy(mbedtls_platform_condition_variable_t*) {}
int cond_signal(mbedtls_platform_condition_variable_t* c) { WakeConditionVariable(c); return 0; }
int cond_broadcast(mbedtls_platform_condition_variable_t* c) { WakeAllConditionVariable(c); return 0; }
int cond_wait(mbedtls_platform_condition_variable_t* c, mbedtls_platform_mutex_t* m) {
    return SleepConditionVariableSRW(c, m, INFINITE, 0) ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR;
}
#else
int mutex_init(mbedtls_platform_mutex_t* m) { return pthread_mutex_init(m, nullptr) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
void mutex_destroy(mbedtls_platform_mutex_t* m) { pthread_mutex_destroy(m); }
int mutex_lock(mbedtls_platform_mutex_t* m) { return pthread_mutex_lock(m) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
int mutex_unlock(mbedtls_platform_mutex_t* m) { return pthread_mutex_unlock(m) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
int cond_init(mbedtls_platform_condition_variable_t* c) { return pthread_cond_init(c, nullptr) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
void cond_destroy(mbedtls_platform_condition_variable_t* c) { pthread_cond_destroy(c); }
int cond_signal(mbedtls_platform_condition_variable_t* c) { return pthread_cond_signal(c) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
int cond_broadcast(mbedtls_platform_condition_variable_t* c) { return pthread_cond_broadcast(c) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR; }
int cond_wait(mbedtls_platform_condition_variable_t* c, mbedtls_platform_mutex_t* m) {
    return pthread_cond_wait(c, m) == 0 ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR;
}
#endif

} // namespace
#endif

namespace mbedtls_cpp {

namespace {
const unsigned char* bytes(std::string_view s) { return reinterpret_cast<const unsigned char*>(s.data()); }
}  // namespace

// ---- init ----

void init() {
    // call_once: the registration and psa_crypto_init() run exactly once, even when called from several threads.
    // ponytail: mbedtls_threading_free_alt() is never called, the global mutexes live as long as the process.
    // Upgrade path: an explicit shutdown, once every thread that may use Mbed TLS has been joined.
    static std::once_flag once;
    static psa_status_t status = PSA_SUCCESS;
    std::call_once(once, [] {
#if defined(MBEDTLS_THREADING_ALT)
        mbedtls_threading_set_alt(mutex_init, mutex_destroy, mutex_lock, mutex_unlock, cond_init, cond_destroy,
                                  cond_signal, cond_broadcast, cond_wait);
#endif
        status = psa_crypto_init();
    });
    if (status != PSA_SUCCESS) throw Error("psa_crypto_init() failed with status " + std::to_string(status), status);
}

// ---- crypto ----
namespace {

struct Hash {
    std::string_view name;
    psa_algorithm_t alg;
};

constexpr Hash kHashes[] = {
    {"md5", PSA_ALG_MD5},           {"sha1", PSA_ALG_SHA_1},         {"sha224", PSA_ALG_SHA_224},
    {"sha256", PSA_ALG_SHA_256},    {"sha384", PSA_ALG_SHA_384},     {"sha512", PSA_ALG_SHA_512},
    {"sha3_224", PSA_ALG_SHA3_224}, {"sha3_256", PSA_ALG_SHA3_256},  {"sha3_384", PSA_ALG_SHA3_384},
    {"sha3_512", PSA_ALG_SHA3_512},
};

constexpr size_t kMaxDigestSize = 64;  // SHA-512, SHA3-512

psa_algorithm_t find(std::string_view name) {
    for (const Hash& h : kHashes)
        if (h.name == name) return h.alg;
    throw UnknownAlgorithm(name);
}

[[noreturn]] void fail(const char* what, psa_status_t status) {
    throw Error(std::string(what) + " failed with PSA status " + std::to_string(status), status);
}


}  // namespace

std::vector<std::string> hash_algorithms() {
    std::vector<std::string> names;
    for (const Hash& h : kHashes) names.emplace_back(h.name);
    return names;
}

Digest::Digest(std::string_view name) : name_(name), alg_(find(name)) {}

size_t Digest::size() const noexcept { return PSA_HASH_LENGTH(alg_); }

std::string Digest::compute(std::string_view data) const {
    init();
    unsigned char out[kMaxDigestSize];
    size_t len = 0;
    const psa_status_t status = psa_hash_compute(alg_, bytes(data), data.size(), out, sizeof out, &len);
    if (status != PSA_SUCCESS) fail("digest", status);
    return std::string(reinterpret_cast<const char*>(out), len);
}

Hmac::Hmac(std::string_view name, std::string_view key) : digest_(name), key_(key) {}

Hmac::~Hmac() { mbedtls_platform_zeroize(key_.data(), key_.size()); }

std::string Hmac::compute(std::string_view data) const {
    init();
    // PSA refuses to import an empty key. HMAC zero-pads the key to the block size, so the empty key and a single
    // zero byte give the same result.
    static const unsigned char zero = 0;
    const unsigned char* key = key_.empty() ? &zero : bytes(key_);
    const size_t key_len = key_.empty() ? 1 : key_.size();

    // ponytail: the key is imported (and destroyed) on every call, a volatile PSA key costs a few microseconds and
    // keeps concurrent calls independent. Upgrade path: hold the key id in this object, once an incremental API exists
    // (it needs its own lock to be shared between threads).
    const psa_algorithm_t alg = PSA_ALG_HMAC(find(digest_.name()));
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, alg);

    psa_key_id_t id = 0;
    psa_status_t status = psa_import_key(&attr, key, key_len, &id);
    if (status != PSA_SUCCESS) fail("hmac", status);
    unsigned char out[kMaxDigestSize];
    size_t len = 0;
    status = psa_mac_compute(id, alg, bytes(data), data.size(), out, sizeof out, &len);
    psa_destroy_key(id);
    if (status != PSA_SUCCESS) fail("hmac", status);
    return std::string(reinterpret_cast<const char*>(out), len);
}

std::string digest(std::string_view name, std::string_view data) { return Digest(name).compute(data); }

std::string hmac(std::string_view name, std::string_view key, std::string_view data) {
    return Hmac(name, key).compute(data);
}

std::string random_bytes(size_t n) {
    init();
    std::string out(n, '\0');
    if (n == 0) return out;
    const psa_status_t status = psa_generate_random(reinterpret_cast<unsigned char*>(out.data()), n);
    if (status != PSA_SUCCESS) fail("random_bytes", status);
    return out;
}

// ---- x509 ----
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
