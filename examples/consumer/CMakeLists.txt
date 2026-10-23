cmake_minimum_required(VERSION 3.26)
project(mbedtls_cpp_consumer CXX)

# cmake -S examples/consumer -B build -DCMAKE_PREFIX_PATH=<install prefix or unpacked prebuilt archive>
find_package(mbedtls_cpp CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE mbedtls::cpp)

enable_testing()
add_test(NAME consumer COMMAND consumer)
