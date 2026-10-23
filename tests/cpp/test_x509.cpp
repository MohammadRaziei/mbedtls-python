#include <mbedtls_cpp_x509.h>

#include <atomic>
#include <thread>
#include <vector>

#include "common.hpp"
#include "utest.h"

// Chain leaf -> int -> root from tests/data (regenerate with tests/gen_x509_fixtures.sh).
struct Chain {
    std::string leaf = der_of("leaf.pem"), inter = der_of("int.pem"), root = der_of("root.pem");
    const unsigned char* chain[2] = {bytes(leaf), bytes(inter)};
    size_t chain_len[2] = {leaf.size(), inter.size()};
    const unsigned char* anchor[1] = {bytes(root)};
    size_t anchor_len[1] = {root.size()};
};

UTEST(x509, fixtures_parse) {
    Chain c;
    ASSERT_FALSE(c.leaf.empty());
    ASSERT_FALSE(c.inter.empty());
    ASSERT_FALSE(c.root.empty());
}

UTEST(x509, verify_complete_chain) {
    Chain c;
    uint32_t flags = 0;
    char info[512];
    ASSERT_EQ(mbedtls_cpp_x509_verify(c.chain, c.chain_len, 2, c.anchor, c.anchor_len, 1, "leaf.example.test", &flags,
                                      info, sizeof info), 0);
}

UTEST(x509, verify_wrong_hostname_fails) {
    Chain c;
    uint32_t flags = 0;
    char info[512];
    ASSERT_EQ(mbedtls_cpp_x509_verify(c.chain, c.chain_len, 2, c.anchor, c.anchor_len, 1, "other.example.test", &flags,
                                      info, sizeof info), MBEDTLS_CPP_X509_VERIFY_FAILED);
}

// Without the intermediate the chain must be refused, and say why.
UTEST(x509, verify_incomplete_chain_fails) {
    Chain c;
    uint32_t flags = 0;
    char info[512];
    ASSERT_EQ(mbedtls_cpp_x509_verify(c.chain, c.chain_len, 1, c.anchor, c.anchor_len, 1, nullptr, &flags, info,
                                      sizeof info), MBEDTLS_CPP_X509_VERIFY_FAILED);
    ASSERT_NE(flags, (uint32_t)0);
    ASSERT_NE(info[0], '\0');
}

UTEST(x509, concurrent_verify) {
    Chain c;
    std::atomic<int> failures{0};
    auto work = [&] {
        for (int i = 0; i < 5; ++i) {
            uint32_t flags = 0;
            char info[512];
            if (mbedtls_cpp_x509_verify(c.chain, c.chain_len, 2, c.anchor, c.anchor_len, 1, "leaf.example.test", &flags,
                                        info, sizeof info) != 0)
                ++failures;
            if (mbedtls_cpp_x509_verify(c.chain, c.chain_len, 1, c.anchor, c.anchor_len, 1, nullptr, &flags, info,
                                        sizeof info) != MBEDTLS_CPP_X509_VERIFY_FAILED)
                ++failures;
        }
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back(work);
    for (auto& t : threads) t.join();
    ASSERT_EQ(failures.load(), 0);
}
