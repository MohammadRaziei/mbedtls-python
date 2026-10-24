// Smallest possible user of the installed package: SHA-256("abc") and an HMAC through the C++ classes.
#include <mbedtls.hpp>

#include <cstdio>

int main() {
    const std::string sum = mbedtls::digest("sha256", "abc");
    for (unsigned char c : sum) std::printf("%02x", c);
    std::printf("\n");

    const mbedtls::Hmac mac("sha256", "key");  // reusable: same key, many messages
    return sum.size() == 32 && static_cast<unsigned char>(sum[0]) == 0xba && mac.compute("a") != mac.compute("b") ? 0 : 1;
}
