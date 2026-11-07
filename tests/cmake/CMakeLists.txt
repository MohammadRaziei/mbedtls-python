# End-to-end checks of the two ways to get the SDK, both building examples/consumer and running it:
#   python  "mbedtls.get_cmake_dir() -> find_package(mbedtls_cpp)" against the installed Python package
#   prefix  `cmake --install` of this build tree, then find_package(mbedtls_cpp) with CMAKE_PREFIX_PATH
set(MBEDTLS_CPP_CMAKE_TEST_EXAMPLE_DIR ${PROJECT_SOURCE_DIR}/examples/consumer)
set(MBEDTLS_CPP_CMAKE_TEST_BUILD_DIR ${CMAKE_BINARY_DIR}/cmake_consumer_test)

foreach(_mode python prefix)
    add_test(NAME test_mbedtls_cmake_${_mode}
        COMMAND ${CMAKE_COMMAND}
            -DMODE=${_mode}
            -DPYTHON_INSTALL_DIR=${PYTHON_INSTALL_DIR}
            -DMAIN_BINARY_DIR=${CMAKE_BINARY_DIR}
            -DEXAMPLE_SOURCE_DIR=${MBEDTLS_CPP_CMAKE_TEST_EXAMPLE_DIR}
            -DEXAMPLE_BINARY_DIR=${MBEDTLS_CPP_CMAKE_TEST_BUILD_DIR}-${_mode}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_consumer_test.cmake
    )
endforeach()
