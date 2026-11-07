cmake_minimum_required(VERSION 3.26)
project(mbedtls_cpp_consumer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put `consumer` at a predictable path on every generator (multi-config included).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} ${CMAKE_BINARY_DIR})
endforeach()

# Three ways to get mbedtls::cpp:
#   cmake -S examples/consumer -B build -DCMAKE_PREFIX_PATH=<install prefix>      (cmake --install of this project)
#   cmake -S examples/consumer -B build -DMBEDTLS_CPP_FROM_PYTHON=ON              (pip install mbedtls: the nanobind pattern)
#   add_subdirectory(examples/consumer) from this project (MBEDTLS_CPP_BUILD_EXAMPLES=ON): the target already exists
if(NOT TARGET mbedtls::cpp)
    if(MBEDTLS_CPP_FROM_PYTHON)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        execute_process(
            COMMAND ${Python3_EXECUTABLE} -m mbedtls --cmake-dir
            OUTPUT_VARIABLE mbedtls_cpp_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY)
    endif()
    find_package(mbedtls_cpp CONFIG REQUIRED)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE mbedtls::cpp)

# Windows has no rpath: consumer.exe finds mbedtls_cpp.dll only next to itself (or on PATH). Elsewhere the imported
# target already gives the program an rpath to libmbedtls_cpp.so / libmbedtls_cpp.dylib.
if(WIN32)
    add_custom_command(TARGET consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mbedtls::cpp> $<TARGET_FILE_DIR:consumer>)
endif()

enable_testing()
add_test(NAME consumer COMMAND consumer)
