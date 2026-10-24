#pragma once
// Hashes, HMAC and random bytes over PSA. Data goes in as std::string_view and comes out as std::string: both are
// plain byte containers here, embedded NULs included.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "error.hpp"

namespace mbedtls_cpp {

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

}  // namespace mbedtls_cpp
