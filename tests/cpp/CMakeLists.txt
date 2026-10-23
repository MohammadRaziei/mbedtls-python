# Same layout as liburlparser/httpp: tests/utest (utest.h, public domain) + one test_*.cpp per area.
file(GLOB _tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
add_executable(test_mbedtls_cpp ${_tests} ${PROJECT_SOURCE_DIR}/tests/utest/utest_main.cxx)
target_include_directories(test_mbedtls_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/tests/utest)
target_link_libraries(test_mbedtls_cpp PRIVATE mbedtls::cpp)
target_compile_definitions(test_mbedtls_cpp PRIVATE MBEDTLS_CPP_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
if(WIN32)
    # utest.h includes <Windows.h>; keep it from dragging in the legacy <winsock.h>.
    target_compile_definitions(test_mbedtls_cpp PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
add_test(NAME test_mbedtls_cpp COMMAND test_mbedtls_cpp)
