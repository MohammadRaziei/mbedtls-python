cmake_minimum_required(VERSION 3.26)
project(mbedtls_cpp_via_python CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# `pip install mbedtls`, then ask the installed package where its CMake config lives (the nanobind pattern).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m mbedtls --cmake-dir
    OUTPUT_VARIABLE mbedtls_cpp_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
find_package(mbedtls_cpp CONFIG REQUIRED)

# ponytail: same program as examples/consumer, not copied.
add_executable(app ${CMAKE_CURRENT_LIST_DIR}/../consumer/main.cpp)
target_link_libraries(app PRIVATE mbedtls_cpp::core)

enable_testing()
add_test(NAME app COMMAND app)
