cmake_minimum_required(VERSION 3.26)
project(mbedtls_python LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

if(DEFINED SKBUILD)
    set(_default_tests OFF)
else()
    set(_default_tests ${PROJECT_IS_TOP_LEVEL})
endif()
option(MBEDTLS_PY_BUILD_TESTS "Build the C++ smoke test" ${_default_tests})
option(MBEDTLS_PY_THREADING_ALT "Use the alt threading layer (always on for Windows) instead of pthread" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Mbed TLS (git submodule, built from source, linked statically) ---
# Static libs end up inside the Python extension (a shared object), so they must be PIC. Hidden visibility keeps
# every mbedtls symbol out of the .so's exports: another copy in the same process (httpp, python-mbedtls) cannot clash.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
# mbedtls's own install() rules are unconditional unless this is forced ON; nothing from it belongs in our install.
set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON CACHE BOOL "" FORCE)

# The codegen'd sources (library/error.c, psa_crypto_driver_wrappers_no_static.c, ...) must exist before
# add_subdirectory(). Mbed TLS generates them itself only on Linux/macOS (GEN_FILES defaults OFF on Windows, where it
# assumes a release tarball), and a git submodule never ships them, so we always run its own scripts here
# (https://github.com/Mbed-TLS/mbedtls/issues/10678). Needs Python 3 (+ jsonschema, jinja2) and Perl on PATH.
set(_mbedtls_dir ${PROJECT_SOURCE_DIR}/src/third_party/mbedtls)
if(NOT EXISTS ${_mbedtls_dir}/tf-psa-crypto/CMakeLists.txt)
    message(FATAL_ERROR "Mbed TLS submodule is empty: run `git submodule update --init --recursive`")
endif()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} framework/scripts/make_generated_files.py
    WORKING_DIRECTORY ${_mbedtls_dir}/tf-psa-crypto
    RESULT_VARIABLE _gen_result)
if(NOT _gen_result EQUAL 0)
    message(FATAL_ERROR "Failed to generate tf-psa-crypto sources (exit ${_gen_result})")
endif()
execute_process(
    COMMAND ${Python3_EXECUTABLE} scripts/make_generated_files.py
    WORKING_DIRECTORY ${_mbedtls_dir}
    RESULT_VARIABLE _gen_result)
if(NOT _gen_result EQUAL 0)
    message(FATAL_ERROR "Failed to generate mbedtls sources (exit ${_gen_result})")
endif()

# Thread safety: see cmake/mbedtls_config.h.
set(TF_PSA_CRYPTO_USER_CONFIG_FILE ${PROJECT_SOURCE_DIR}/cmake/mbedtls_config.h CACHE FILEPATH "" FORCE)
find_package(Threads REQUIRED)
set(LINK_WITH_PTHREAD ON CACHE BOOL "" FORCE)
if(MBEDTLS_PY_THREADING_ALT)
    add_compile_definitions(MBEDTLS_PY_THREADING_ALT)
endif()
# threading_alt.h must be found by every Mbed TLS source (it builds several object libraries that do not see each
# other's include paths) and by everything that includes its headers: a directory-level include path reaches all.
if(WIN32 OR MBEDTLS_PY_THREADING_ALT)
    include_directories(${PROJECT_SOURCE_DIR}/src/core/alt)
endif()
add_subdirectory(${_mbedtls_dir} EXCLUDE_FROM_ALL)

# One target for everything downstream: Mbed TLS + the threading registration.
add_library(mbedtls_py_core STATIC src/core/threading.cpp)
target_include_directories(mbedtls_py_core PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_link_libraries(mbedtls_py_core PUBLIC mbedtls mbedx509 tfpsacrypto Threads::Threads)
add_library(mbedtls_py::core ALIAS mbedtls_py_core)

if(MBEDTLS_PY_BUILD_TESTS)
    enable_testing()
    add_executable(mbedtls_py_smoke tests/cpp/smoke.cpp)
    target_link_libraries(mbedtls_py_smoke PRIVATE mbedtls_py::core)
    add_test(NAME smoke COMMAND mbedtls_py_smoke)
endif()

# --- Python extension (only under scikit-build-core, i.e. `pip install .` / cibuildwheel) ---
if(DEFINED SKBUILD)
    # ${SKBUILD_SABI_COMPONENT} is Development.SABIModule when building a stable-ABI wheel (wheel.py-api in
    # pyproject.toml) and empty otherwise (free-threaded builds cannot use abi3).
    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module ${SKBUILD_SABI_COMPONENT})
    find_package(Cython MODULE REQUIRED VERSION 3.1)
    include(UseCython)

    cython_transpile(${PROJECT_SOURCE_DIR}/src/mbedtls/_mbedtls.pyx LANGUAGE CXX OUTPUT_VARIABLE _mbedtls_cxx)
    if(NOT "${SKBUILD_SABI_VERSION}" STREQUAL "")
        set(USE_SABI USE_SABI ${SKBUILD_SABI_VERSION})
    endif()
    if(SKBUILD_SOABI)
        set(Python_SOABI ${SKBUILD_SOABI})
    endif()
    python_add_library(_mbedtls MODULE "${_mbedtls_cxx}" WITH_SOABI ${USE_SABI})
    target_link_libraries(_mbedtls PRIVATE mbedtls_py::core)
    if(UNIX AND NOT APPLE) # belt and braces next to hidden visibility
        target_link_options(_mbedtls PRIVATE -Wl,--exclude-libs,ALL)
    endif()

    install(TARGETS _mbedtls DESTINATION mbedtls)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/mbedtls/ DESTINATION mbedtls FILES_MATCHING PATTERN "*.py")
endif()
