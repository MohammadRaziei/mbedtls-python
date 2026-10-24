#pragma once
// Exceptions. They derive from std types on purpose: Cython maps std::invalid_argument to ValueError and
// std::runtime_error to RuntimeError, so the Python layer needs no translation code.
#include <stdexcept>
#include <string>
#include <string_view>

namespace mbedtls_cpp {

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

}  // namespace mbedtls_cpp
