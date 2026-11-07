cmake_minimum_required(VERSION 3.26)
project(mbedtls_cpp_via_python CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# ponytail: same program as examples/consumer, not copied.
add_executable(app ${CMAKE_CURRENT_LIST_DIR}/../consumer/main.cpp)
target_link_libraries(app PRIVATE mbedtls::cpp)

# Windows has no rpath: the program finds mbedtls_cpp_core.dll only next to itself (or on PATH). Elsewhere the imported
# target already gives it an rpath to the shared library.
if(WIN32)
    add_custom_command(TARGET app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mbedtls::cpp> $<TARGET_FILE_DIR:app>)
endif()

enable_testing()
add_test(NAME app COMMAND app)
