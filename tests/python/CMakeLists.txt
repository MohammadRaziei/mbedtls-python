find_package(Python3 COMPONENTS Interpreter REQUIRED)

execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import pytest"
    OUTPUT_QUIET ERROR_QUIET
    RESULT_VARIABLE MBEDTLS_CPP_PYDEPS_RESULT
)
if(NOT MBEDTLS_CPP_PYDEPS_RESULT EQUAL 0)
    message(WARNING "pytest not installed: Python tests skipped (pip install pytest)")
    return()
endif()

# Run from the install dir so `import mbedtls` finds the freshly built package.
add_test(NAME test_mbedtls_python
    COMMAND ${Python3_EXECUTABLE} -m pytest ${MBEDTLS_CPP_TEST_SOURCE_DIR}/python -p no:cacheprovider
    WORKING_DIRECTORY ${PYTHON_INSTALL_DIR}
)
