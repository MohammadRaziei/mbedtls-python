cmake_minimum_required(VERSION 3.26)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(PyProject)  # PyProject_NAME / _VERSION / _DESCRIPTION, read from pyproject.toml: the one version of the project

project(mbedtls_python VERSION ${PyProject_VERSION} LANGUAGES C CXX)
message(STATUS "Project: ${PROJECT_NAME}@v${PROJECT_VERSION}")
include(Startup)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(Optimize)

option(MBEDTLS_CPP_BUILD_TESTS "Build the C++ tests, and register the Python and CMake-consumer tests" ${PROJECT_IS_TOP_LEVEL})
option(MBEDTLS_CPP_BUILD_PYTHON "Build the Python bindings" ${PROJECT_IS_TOP_LEVEL})
option(MBEDTLS_CPP_BUILD_EXAMPLES "Build the C++ example in examples/consumer" OFF)
option(MBEDTLS_CPP_THREADING_ALT "Use the alt threading layer (always on for Windows) instead of pthread" OFF)

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
if(MBEDTLS_CPP_THREADING_ALT)
    add_compile_definitions(MBEDTLS_CPP_THREADING_ALT)
endif()
# threading_alt.h must be found by every Mbed TLS source (it builds several object libraries that do not see each
# other's include paths) and by everything that includes its headers: a directory-level include path reaches all.
if(WIN32 OR MBEDTLS_CPP_THREADING_ALT)
    include_directories(${PROJECT_SOURCE_DIR}/src/alt)
endif()
add_subdirectory(${_mbedtls_dir} EXCLUDE_FROM_ALL)

# --- mbedtls_cpp C++ core ---
# The core is always a shared library (libmbedtls_cpp_core.so, libmbedtls_cpp_core.dylib, mbedtls_cpp_core.dll): the Python
# extension and every C++ program link to the same copy, which is what lets a wheel carry one engine. Mbed TLS is
# linked statically *into* it, with hidden visibility: only what include/mbedtls.hpp marks MBEDTLS_CPP_API is exported
# (include/mbedtls_cpp/export.hpp), so no Mbed TLS symbol leaks and another copy in the same process cannot clash.
set(MBEDTLS_CPP_CORE_SOURCES ${PROJECT_SOURCE_DIR}/src/mbedtls.cpp)

add_library(mbedtls_cpp_core SHARED ${MBEDTLS_CPP_CORE_SOURCES})
target_compile_definitions(mbedtls_cpp_core PRIVATE MBEDTLS_CPP_BUILDING_LIBRARY)
target_compile_features(mbedtls_cpp_core PUBLIC cxx_std_17)  # mbedtls.hpp uses std::string_view
add_library(mbedtls::cpp ALIAS mbedtls_cpp_core)

set_target_properties(mbedtls_cpp_core PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    EXPORT_NAME cpp                     # consumers link mbedtls::cpp
)
if(NOT DEFINED SKBUILD)
    # libmbedtls_cpp_core.so -> ...so.0.1 -> ...so.0.1.0 in a plain install. A wheel carries a
    # single file (zip has no symlinks), so it gets no version suffix.
    set_target_properties(mbedtls_cpp_core PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )
endif()
# MSVC: the exported classes hold std::string / std::vector members (C4251) and derive from std exceptions (C4275).
# Both are fine as long as the library and its users use the same compiler and runtime, which a DLL of C++ classes
# already requires.
target_compile_options(mbedtls_cpp_core PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/wd4251 /wd4275>)
if(UNIX AND NOT APPLE) # belt and braces next to hidden visibility
    target_link_options(mbedtls_cpp_core PRIVATE -Wl,--exclude-libs,ALL)
endif()

# Install layout: the wheel carries the C++ SDK as a regular install prefix, mbedtls/sdk/ (include/, lib/ and
# lib/cmake/MbedTLS/ inside it: the same tree `cmake --install` makes; see get_sdk_dir() in the Python package).
# A plain `cmake --install` uses the usual prefix layout.
if(DEFINED SKBUILD)
    set(_mbcpp_sdk_dest   mbedtls/sdk)
    set(_mbcpp_lib_dest   ${_mbcpp_sdk_dest}/lib)
    set(_mbcpp_bin_dest   ${_mbcpp_sdk_dest}/lib)  # mbedtls_cpp_core.dll beside mbedtls_cpp_core.lib; mbedtls/__init__.py adds it with os.add_dll_directory
    set(_mbcpp_inc_dest   ${_mbcpp_sdk_dest}/include)
    set(_mbcpp_cmake_dest ${_mbcpp_sdk_dest}/lib/cmake/MbedTLS)
else()
    include(GNUInstallDirs)
    set(_mbcpp_lib_dest   ${CMAKE_INSTALL_LIBDIR})
    set(_mbcpp_bin_dest   ${CMAKE_INSTALL_BINDIR})
    set(_mbcpp_inc_dest   ${CMAKE_INSTALL_INCLUDEDIR})
    set(_mbcpp_cmake_dest ${CMAKE_INSTALL_LIBDIR}/cmake/MbedTLS)
endif()

target_include_directories(mbedtls_cpp_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${_mbcpp_inc_dest}>
)

# Everything that decides how the core sources are compiled lives in this function, so a target that compiles them
# itself (tests/cpp does: the tests call the Mbed TLS / PSA C API directly, which the shared library does not export)
# gets exactly the same settings as mbedtls_cpp_core.
function(mbedtls_cpp_apply_core_build_settings target)
    # A target that compiles the core sources itself has no library to import from: MBEDTLS_CPP_API must be empty
    # there. mbedtls_cpp_core builds with MBEDTLS_CPP_BUILDING_LIBRARY instead.
    # (mbedtls_cpp_core also has its include directory, with the install-tree variant, set above.)
    if(NOT target STREQUAL "mbedtls_cpp_core")
        target_compile_definitions(${target} PUBLIC MBEDTLS_CPP_STATIC_DEFINE)
        target_include_directories(${target} PUBLIC ${PROJECT_SOURCE_DIR}/include)
    endif()
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
    )
    apply_optimization_flags(${target})
endfunction()

mbedtls_cpp_apply_core_build_settings(mbedtls_cpp_core)
target_link_libraries(mbedtls_cpp_core PRIVATE mbedtls mbedx509 tfpsacrypto Threads::Threads)

# --- install (find_package(MbedTLS CONFIG): mbedtls::cpp) ---
# Two consumers: `cmake --install build --prefix <dir>` / `cpack` (see .github/workflows/prebuilt.yml), and the wheel,
# which carries the same package inside mbedtls/sdk/ so that `pip install mbedtls` + `python -m mbedtls --cmake-dir` is
# all a C++ project needs (mbedtls.get_cmake_dir() / get_include_dir() / get_lib_dir() / get_sdk_dir()).
if(PROJECT_IS_TOP_LEVEL)
    include(CMakePackageConfigHelpers)
    # The export set is not called MbedTLSTargets: Mbed TLS's own install rules use that name, and its targets would end up in ours.
    install(TARGETS mbedtls_cpp_core EXPORT mbedtls_cpp_core_export
        LIBRARY DESTINATION ${_mbcpp_lib_dest}     # libmbedtls_cpp_core.so, libmbedtls_cpp_core.dylib
        RUNTIME DESTINATION ${_mbcpp_bin_dest}     # mbedtls_cpp_core.dll
        ARCHIVE DESTINATION ${_mbcpp_lib_dest}     # mbedtls_cpp_core.lib, the import library on Windows
    )
    install(DIRECTORY include/ DESTINATION ${_mbcpp_inc_dest})
    install(EXPORT mbedtls_cpp_core_export FILE MbedTLSTargets.cmake NAMESPACE mbedtls:: DESTINATION ${_mbcpp_cmake_dest})
    configure_package_config_file(cmake/MbedTLSConfig.cmake.in ${PROJECT_BINARY_DIR}/MbedTLSConfig.cmake
        INSTALL_DESTINATION ${_mbcpp_cmake_dest})
    # The package answers to find_package(MbedTLS <version>), so it reports Mbed TLS's version, not this project's.
    file(STRINGS ${_mbedtls_dir}/include/mbedtls/build_info.h _mbedtls_ver_line REGEX "define MBEDTLS_VERSION_STRING ")
    string(REGEX REPLACE ".*\"([0-9]+\\.[0-9]+\\.[0-9]+)\".*" "\\1" _mbedtls_version "${_mbedtls_ver_line}")
    write_basic_package_version_file(${PROJECT_BINARY_DIR}/MbedTLSConfigVersion.cmake
        VERSION ${_mbedtls_version} COMPATIBILITY SameMajorVersion)
    install(FILES ${PROJECT_BINARY_DIR}/MbedTLSConfig.cmake ${PROJECT_BINARY_DIR}/MbedTLSConfigVersion.cmake
            DESTINATION ${_mbcpp_cmake_dest})

    if(NOT DEFINED SKBUILD)
        # ponytail: the archive is only as portable as the runner that built it (glibc of ubuntu-latest, macOS arch of
        # macos-latest, no universal2). Upgrade path: build the Linux one in a manylinux container.
        set(CPACK_PACKAGE_NAME mbedtls-cpp-core)
        set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
        set(CPACK_PACKAGE_FILE_NAME mbedtls-cpp-core-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
        if(WIN32)
            set(CPACK_GENERATOR ZIP)
        else()
            set(CPACK_GENERATOR TGZ)
        endif()
        include(CPack)
    endif()
endif()

if(MBEDTLS_CPP_BUILD_PYTHON OR DEFINED SKBUILD)
    set(PYTHON_INSTALL_DIR ${CMAKE_BINARY_DIR}/python/install)
    add_subdirectory(${PROJECT_SOURCE_DIR}/src/bindings/python)
    if(DEFINED SKBUILD)
        return()
    endif()
endif()

if(MBEDTLS_CPP_BUILD_TESTS)
    enable_testing()
    set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
    add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
endif()

# The C++ example (examples/consumer), against the same library the users link.
if(MBEDTLS_CPP_BUILD_EXAMPLES)
    add_subdirectory(${PROJECT_SOURCE_DIR}/examples/consumer)
endif()
