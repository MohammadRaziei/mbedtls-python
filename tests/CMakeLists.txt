set(MBEDTLS_CPP_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(${MBEDTLS_CPP_TEST_SOURCE_DIR}/cpp)

# The Python and CMake-consumer tests run against the package the build_python target installed in PYTHON_INSTALL_DIR:
# the same mbedtls/sdk layout as the wheel.
if(TARGET build_python)
    add_subdirectory(${MBEDTLS_CPP_TEST_SOURCE_DIR}/python)
    add_subdirectory(${MBEDTLS_CPP_TEST_SOURCE_DIR}/cmake)
endif()
