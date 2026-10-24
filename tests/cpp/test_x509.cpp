#include <mbedtls.hpp>

#include <atomic>
#include <thread>
#include <vector>

#include "common.hpp"
#include "utest.h"

using namespace mbedtls_cpp;

// Chain leaf -> int -> root from tests/data (regenerate with tests/gen_x509_fixtures.sh).
struct Chain {
    Certificate leaf = load("leaf.pem"), inter = load("int.pem"), root = load("root.pem");
};

UTEST(x509, fields) {
    Chain c;
    ASSERT_EQ(c.leaf.version(), 3);
    ASSERT_FALSE(c.leaf.is_ca());
    ASSERT_TRUE(c.inter.is_ca());
    ASSERT_TRUE(c.root.is_ca());
    ASSERT_TRUE(c.leaf.issuer() == c.inter.subject());
    ASSERT_TRUE(c.root.subject() == c.root.issuer());
    ASSERT_FALSE(c.leaf.serial().empty());
    ASSERT_TRUE(c.leaf.not_before().year < c.leaf.not_after().year);
}

UTEST(x509, der_round_trip_and_equality) {
    Chain c;
    const Certificate again = Certificate::from_der(c.leaf.der());
    ASSERT_TRUE(again == c.leaf);
    ASSERT_TRUE(again != c.root);
}

UTEST(x509, bad_input_throws) {
    Chain c;
    ASSERT_TRUE(throws<CertificateError>([] { Certificate::from_der("not a certificate"); }));
    ASSERT_TRUE(throws<CertificateError>([&] { Certificate::from_der(c.leaf.der() + "junk"); }));
    ASSERT_TRUE(throws<CertificateError>([] { Certificate::from_pem("-----BEGIN CERTIFICATE-----\nAAAA\n-----END CERTIFICATE-----\n"); }));
}

UTEST(x509, pem_bundle_keeps_order) {
    const auto certs = Certificate::from_pem(read_file("leaf.pem") + read_file("int.pem"));
    ASSERT_EQ(certs.size(), (size_t)2);
    ASSERT_TRUE(certs[1].is_ca());
}

UTEST(x509, verify_complete_chain) {
    Chain c;
    ASSERT_TRUE(c.leaf.verify({c.root}, {c.inter}, "leaf.example.test").ok);
}

UTEST(x509, verify_wrong_hostname_fails) {
    Chain c;
    const VerifyResult r = c.leaf.verify({c.root}, {c.inter}, "other.example.test");
    ASSERT_FALSE(r.ok);
    ASSERT_NE(r.flags, (uint32_t)0);
}

// Without the intermediate the chain must be refused, and say why.
UTEST(x509, verify_incomplete_chain_fails) {
    Chain c;
    const VerifyResult r = c.leaf.verify({c.root});
    ASSERT_FALSE(r.ok);
    ASSERT_NE(r.flags, (uint32_t)0);
    ASSERT_FALSE(r.info.empty());
}

UTEST(x509, verify_without_anchors_fails) {
    Chain c;
    ASSERT_FALSE(c.leaf.verify({}, {c.inter}).ok);
}

UTEST(x509, hostname_with_nul_throws) {
    Chain c;
    ASSERT_TRUE(throws<std::invalid_argument>([&] { c.leaf.verify({c.root}, {c.inter}, std::string_view("a\0b", 3)); }));
}

UTEST(x509, concurrent_verify) {
    Chain c;
    std::atomic<int> failures{0};
    auto work = [&] {
        for (int i = 0; i < 5; ++i) {
            if (!c.leaf.verify({c.root}, {c.inter}, "leaf.example.test").ok) ++failures;
            if (c.leaf.verify({c.root}).ok) ++failures;
        }
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back(work);
    for (auto& t : threads) t.join();
    ASSERT_EQ(failures.load(), 0);
}
