#pragma once
#include <mbedtls_cpp_x509.h>

#include <fstream>
#include <iterator>
#include <string>

inline const unsigned char* bytes(const std::string& s) { return reinterpret_cast<const unsigned char*>(s.data()); }

inline std::string read_file(const char* name) {
    std::ifstream f(std::string(MBEDTLS_CPP_DATA_DIR) + "/" + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

// Parse a PEM certificate from tests/data and hand back its DER (empty on failure).
inline std::string der_of(const char* name) {
    const std::string pem = read_file(name);
    mbedtls_cpp_x509* chain = nullptr;
    std::string der;
    if (mbedtls_cpp_x509_parse(bytes(pem), pem.size(), 1, &chain) == 0) {
        mbedtls_cpp_x509_info info;
        if (mbedtls_cpp_x509_get_info(chain, 0, &info) == 0)
            der.assign(reinterpret_cast<const char*>(info.der), info.der_len);
    }
    mbedtls_cpp_x509_free(chain);
    return der;
}
