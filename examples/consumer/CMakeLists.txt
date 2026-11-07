cmake_minimum_required(VERSION 3.26)
project(mbedtls_cpp_consumer CXX)

# Put the executable at a predictable path on every generator (multi-config included).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} ${CMAKE_BINARY_DIR})
endforeach()

# cmake -S examples/consumer -B build -DCMAKE_PREFIX_PATH=<install prefix or unpacked prebuilt archive>
if(NOT TARGET mbedtls::cpp)  # already there when added with MBEDTLS_CPP_BUILD_EXAMPLES
    find_package(MbedTLS CONFIG REQUIRED)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE mbedtls::cpp)

# Windows has no rpath: the program finds mbedtls_cpp_core.dll only next to itself (or on PATH). Elsewhere the imported
# target already gives it an rpath to the shared library.
if(WIN32)
    add_custom_command(TARGET consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mbedtls::cpp> $<TARGET_FILE_DIR:consumer>)
endif()

enable_testing()
add_test(NAME consumer COMMAND consumer)
