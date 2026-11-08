cmake_minimum_required(VERSION 3.26)
project(mbedtls_consumer C)

# Put the executable at a predictable path on every generator (multi-config included).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} ${CMAKE_BINARY_DIR})
endforeach()

# cmake -S examples/consumer -B build -DCMAKE_PREFIX_PATH=<install prefix or unpacked prebuilt archive>
# (or MbedTLS_DIR=$(python -m mbedtls --cmake-dir)). The package is Mbed TLS's own: MbedTLS::mbedtls, MbedTLS::mbedx509,
# MbedTLS::tfpsacrypto.
if(NOT TARGET MbedTLS::tfpsacrypto)
    find_package(MbedTLS CONFIG REQUIRED)
endif()
find_package(Threads REQUIRED)

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE MbedTLS::tfpsacrypto Threads::Threads)

# Windows: the SDK libraries are static (.lib), nothing to copy. Elsewhere the imported target gives the program an rpath
# to the shared libraries.

enable_testing()
add_test(NAME consumer COMMAND consumer)
