# Same layout as liburlparser/httpp: tests/utest (utest.h, public domain) + one test_*.cpp per area.
#
# The tests call the Mbed TLS / PSA C API directly (test_crypto.cpp), which the shared library keeps hidden, so they
# compile the core sources themselves into a static library with the settings of mbedtls_core. The shared library is
# exercised by the Python tests and by the CMake consumer tests (tests/python, tests/cmake).
add_library(mbedtls_core_static STATIC ${MBEDTLS_CPP_CORE_SOURCES})
mbedtls_cpp_apply_core_build_settings(mbedtls_core_static)
target_link_libraries(mbedtls_core_static PUBLIC mbedtls mbedx509 tfpsacrypto Threads::Threads)

file(GLOB _tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
add_executable(test_mbedtls_cpp ${_tests} ${PROJECT_SOURCE_DIR}/tests/utest/utest_main.cxx)
target_include_directories(test_mbedtls_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/tests/utest)
target_link_libraries(test_mbedtls_cpp PRIVATE mbedtls_core_static)
target_compile_definitions(test_mbedtls_cpp PRIVATE MBEDTLS_CPP_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
if(WIN32)
    # utest.h includes <Windows.h>; keep it from dragging in the legacy <winsock.h>.
    target_compile_definitions(test_mbedtls_cpp PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
add_test(NAME test_mbedtls_cpp COMMAND test_mbedtls_cpp)
