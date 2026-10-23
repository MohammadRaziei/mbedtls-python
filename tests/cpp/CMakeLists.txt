add_executable(mbedtls_py_smoke smoke.cpp)
target_link_libraries(mbedtls_py_smoke PRIVATE mbedtls_py::core)
target_compile_definitions(mbedtls_py_smoke PRIVATE MBEDTLS_PY_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/tests/data")
add_test(NAME smoke COMMAND mbedtls_py_smoke)
