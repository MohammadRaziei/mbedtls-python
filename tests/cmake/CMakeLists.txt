# End-to-end checks of the two ways to get the SDK, each building an example and running it:
#   python  examples/find_mbedtls_via_python: "python -m mbedtls --cmake-dir -> find_package(MbedTLS)" against the
#           installed Python package
#   prefix  examples/consumer: `cmake --install` of this build tree, then find_package(MbedTLS) with CMAKE_PREFIX_PATH
set(MBEDTLS_CPP_CMAKE_TEST_BUILD_DIR ${CMAKE_BINARY_DIR}/cmake_consumer_test)

add_test(NAME test_mbedtls_cmake_python
    COMMAND ${CMAKE_COMMAND}
        -DMODE=python
        -DPYTHON_INSTALL_DIR=${PYTHON_INSTALL_DIR}
        -DMAIN_BINARY_DIR=${CMAKE_BINARY_DIR}
        -DEXAMPLE_SOURCE_DIR=${PROJECT_SOURCE_DIR}/examples/find_mbedtls_via_python
        -DEXAMPLE_BINARY_DIR=${MBEDTLS_CPP_CMAKE_TEST_BUILD_DIR}-python
        -DEXAMPLE_EXE=app
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_consumer_test.cmake
)
add_test(NAME test_mbedtls_cmake_prefix
    COMMAND ${CMAKE_COMMAND}
        -DMODE=prefix
        -DPYTHON_INSTALL_DIR=${PYTHON_INSTALL_DIR}
        -DMAIN_BINARY_DIR=${CMAKE_BINARY_DIR}
        -DEXAMPLE_SOURCE_DIR=${PROJECT_SOURCE_DIR}/examples/consumer
        -DEXAMPLE_BINARY_DIR=${MBEDTLS_CPP_CMAKE_TEST_BUILD_DIR}-prefix
        -DEXAMPLE_EXE=consumer
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_consumer_test.cmake
)
