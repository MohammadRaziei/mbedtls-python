cmake_minimum_required(VERSION 3.26)
project(mbedtls_via_python C)

# Put the executable at a predictable path on every generator (multi-config included).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} ${CMAKE_BINARY_DIR})
endforeach()

# `pip install mbedtls`, then ask the installed package where its CMake config lives (the nanobind pattern).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m mbedtls --cmake-dir
    OUTPUT_VARIABLE MbedTLS_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
find_package(MbedTLS CONFIG REQUIRED)
find_package(Threads REQUIRED)

# ponytail: same program as examples/consumer, not copied.
add_executable(app ${CMAKE_CURRENT_LIST_DIR}/../consumer/main.c)
target_link_libraries(app PRIVATE MbedTLS::tfpsacrypto Threads::Threads)

# Windows: the SDK libraries are static (.lib), nothing to copy. Elsewhere the imported target gives the program an rpath
# to the shared libraries.

enable_testing()
add_test(NAME app COMMAND app)
