#pragma once

// MBEDTLS_CPP_API marks what the mbedtls_cpp shared library exports: the classes and the non-inline
// functions of include/mbedtls.hpp. Everything else in the library is hidden (-fvisibility=hidden),
// Mbed TLS itself included: its C API is not part of the library's interface, so another copy of
// Mbed TLS in the same process cannot clash with the one inside.
//
//   building the shared library      MBEDTLS_CPP_BUILDING_LIBRARY is defined: dllexport on Windows,
//                                    default visibility elsewhere.
//   using the shared library         dllimport on Windows, default visibility elsewhere.
//   sources compiled straight into   MBEDTLS_CPP_STATIC_DEFINE is defined (CMake does it for the
//   a target (no library to use)     C++ tests): MBEDTLS_CPP_API is empty.

#if defined(MBEDTLS_CPP_STATIC_DEFINE)
#define MBEDTLS_CPP_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(MBEDTLS_CPP_BUILDING_LIBRARY)
#define MBEDTLS_CPP_API __declspec(dllexport)
#else
#define MBEDTLS_CPP_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define MBEDTLS_CPP_API __attribute__((visibility("default")))
#else
#define MBEDTLS_CPP_API
#endif
