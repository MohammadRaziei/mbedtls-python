#pragma once
#include <mbedtls.hpp>

#include <fstream>
#include <iterator>
#include <string>

inline std::string read_file(const char* name) {
    std::ifstream f(std::string(MBEDTLS_CPP_DATA_DIR) + "/" + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

// The first certificate of a PEM file in tests/data.
inline mbedtls::Certificate load(const char* name) { return mbedtls::Certificate::from_pem(read_file(name)).at(0); }

// True when f() throws an E (utest has no ASSERT_THROW).
template <class E, class F>
bool throws(F f) {
    try {
        f();
    } catch (const E&) {
        return true;
    } catch (...) {
    }
    return false;
}
