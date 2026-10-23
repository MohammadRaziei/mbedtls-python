add_executable(mbedtls_cpp_smoke smoke.cpp)
target_link_libraries(mbedtls_cpp_smoke PRIVATE mbedtls_cpp::core)
target_compile_definitions(mbedtls_cpp_smoke PRIVATE MBEDTLS_CPP_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/tests/data")
add_test(NAME smoke COMMAND mbedtls_cpp_smoke)
