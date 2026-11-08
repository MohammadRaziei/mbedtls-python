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

option(MBEDTLS_CPP_BUILD_TESTS "Build the C++ tests of the Python shim, and register the Python and CMake-consumer tests" ${PROJECT_IS_TOP_LEVEL})
option(MBEDTLS_CPP_BUILD_PYTHON "Build the Python bindings" ${PROJECT_IS_TOP_LEVEL})
option(MBEDTLS_CPP_BUILD_EXAMPLES "Build the C example in examples/consumer against the Mbed TLS built here" OFF)
option(MBEDTLS_CPP_THREADING_ALT "Use the alt threading layer (always on for Windows) instead of pthread" OFF)

# --- Mbed TLS (git submodule), built as shared libraries and installed as the SDK ---
# The SDK is Mbed TLS itself, nothing wrapped around it: include/mbedtls, include/psa, libmbedtls / libmbedx509 /
# libtfpsacrypto (shared) and lib/cmake/MbedTLS (find_package(MbedTLS) -> MbedTLS::mbedtls, MbedTLS::mbedx509,
# MbedTLS::tfpsacrypto), all written by Mbed TLS's own install rules. In a wheel the prefix is mbedtls/sdk inside the
# package (see get_sdk_dir() in the Python package); a plain `cmake --install` uses the usual prefix layout.
if(DEFINED SKBUILD)
    set(MBEDTLS_CPP_SDK_REL mbedtls/sdk)
    # GNUInstallDirs keeps what is already in the cache, so Mbed TLS's install(... ${CMAKE_INSTALL_LIBDIR}) lands in the sdk.
    set(CMAKE_INSTALL_INCLUDEDIR ${MBEDTLS_CPP_SDK_REL}/include CACHE PATH "" FORCE)
    set(CMAKE_INSTALL_LIBDIR ${MBEDTLS_CPP_SDK_REL}/lib CACHE PATH "" FORCE)
    set(CMAKE_INSTALL_BINDIR ${MBEDTLS_CPP_SDK_REL}/lib CACHE PATH "" FORCE)  # mbedtls.dll beside mbedtls.lib; __init__.py adds it with os.add_dll_directory
else()
    set(MBEDTLS_CPP_SDK_REL "")
endif()

set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
# Shared everywhere but Windows. There Mbed TLS cannot be a DLL: it declares no __declspec(dllexport), and
# CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS only exports functions, while its API also has data symbols other libraries use
# (mbedtls_x509_crt_profile_default, ... -> LNK2019 between mbedtls.dll and mbedx509.dll). So on Windows the SDK holds
# static libraries (mbedtls.lib, mbedx509.lib, tfpsacrypto.lib) and the Python extension has them linked in.
# Upgrade path: upstream export macros.
if(WIN32)
    set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
    set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
else()
    set(USE_STATIC_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
    set(USE_SHARED_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
endif()
set(DISABLE_PACKAGE_CONFIG_AND_INSTALL OFF CACHE BOOL "" FORCE)  # Mbed TLS writes the install and the CMake package

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

# Thread safety: see cmake/mbedtls_config.h. It is a user config appended to the default one, and it is installed with the
# headers (below), because anything that includes the installed headers must see the same configuration.
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
add_subdirectory(${_mbedtls_dir})  # in `all`: the install below needs the libraries built

# A wheel carries one file per library (zip has no symlinks): no version suffix, the soname is plain libmbedtls.so.
# set_property with no value removes the property. set_target_properties(... VERSION "") would leave an empty one, and
# CMake would then name the files (and the exported targets) "libmbedtls.so." with a trailing dot, which breaks
# find_package and auditwheel.
if(DEFINED SKBUILD)
    foreach(_t mbedtls mbedx509 tfpsacrypto)
        set_property(TARGET ${_t} PROPERTY VERSION)
        set_property(TARGET ${_t} PROPERTY SOVERSION)
        # The libraries find each other (libmbedtls.so needs libmbedx509.so, ...) beside themselves. Without this the
        # installed ones have no RUNPATH: it works in Python (the extension loads all three first) but auditwheel
        # resolves every library alone and fails with "required library libmbedx509.so could not be located".
        if(APPLE)
            set_property(TARGET ${_t} PROPERTY INSTALL_RPATH "@loader_path")
        elseif(NOT WIN32)
            set_property(TARGET ${_t} PROPERTY INSTALL_RPATH "$ORIGIN")
        endif()
    endforeach()
endif()

# Mbed TLS puts the user config's absolute path in its targets (a PUBLIC compile definition), which would point an
# installed package into this build tree. Exported, it is the copy installed beside the headers instead.
foreach(_t mbedtls mbedx509 tfpsacrypto)
    get_target_property(_defs ${_t} INTERFACE_COMPILE_DEFINITIONS)
    list(FILTER _defs EXCLUDE REGEX "^TF_PSA_CRYPTO_USER_CONFIG_FILE=")
    list(APPEND _defs
        "$<BUILD_INTERFACE:TF_PSA_CRYPTO_USER_CONFIG_FILE=\"${TF_PSA_CRYPTO_USER_CONFIG_FILE}\">"
        "$<INSTALL_INTERFACE:TF_PSA_CRYPTO_USER_CONFIG_FILE=\"$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}/mbedtls_python_config.h\">")
    set_target_properties(${_t} PROPERTIES INTERFACE_COMPILE_DEFINITIONS "${_defs}")
endforeach()

# Next to the headers: that user config, and threading_alt.h which it includes where the alt threading layer is used.
install(FILES ${PROJECT_SOURCE_DIR}/cmake/mbedtls_config.h RENAME mbedtls_python_config.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${PROJECT_SOURCE_DIR}/src/alt/threading_alt.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# In a wheel only the three libraries are kept, as plain .so / .dylib (static .lib on Windows): Mbed TLS also installs the historical
# libmbedcrypto.* (a second copy of tfpsacrypto), pkg-config files and the TF-PSA-Crypto package, which MbedTLS's own
# package does not need. This runs after the install rules of the submodule above.
if(DEFINED SKBUILD)
    install(CODE "
        set(_lib \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")
        file(GLOB _extra \"\${_lib}/*mbedcrypto*\" \"\${_lib}/*.so.*\" \"\${_lib}/*.so.\" \"\${_lib}/*.*.dylib\")
        if(_extra)
            file(REMOVE \${_extra})
        endif()
        file(REMOVE_RECURSE \"\${_lib}/pkgconfig\" \"\${_lib}/cmake/TF-PSA-Crypto\")
    ")
endif()

# --- the private C++ shim of the Python extension ---
# src/bindings/python/shim: small C++ classes over the Mbed TLS / PSA C API (hashes, HMAC, random bytes, X.509) that
# the Cython module wraps. It is compiled into the extension (and into the C++ tests); it is not installed and not part
# of the SDK. Everything it calls is the public C API of the libraries above.
set(MBEDTLS_CPP_SHIM_DIR ${PROJECT_SOURCE_DIR}/src/bindings/python/shim)
set(MBEDTLS_CPP_SHIM_SOURCES ${MBEDTLS_CPP_SHIM_DIR}/mbedtls_shim.cpp)

function(mbedtls_cpp_apply_shim_build_settings target)
    target_include_directories(${target} PUBLIC ${MBEDTLS_CPP_SHIM_DIR})
    target_compile_features(${target} PUBLIC cxx_std_17)  # string_view
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
    )
    apply_optimization_flags(${target})
endfunction()

# --- install: Mbed TLS's own rules above; here only what the whole build adds (a plain `cmake --install`) ---
if(PROJECT_IS_TOP_LEVEL AND NOT DEFINED SKBUILD)
    # ponytail: the archive is only as portable as the runner that built it (glibc of ubuntu-latest, macOS arch of
    # macos-latest, no universal2). Upgrade path: build the Linux one in a manylinux container.
    set(CPACK_PACKAGE_NAME mbedtls)
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_FILE_NAME mbedtls-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
    if(WIN32)
        set(CPACK_GENERATOR ZIP)
    else()
        set(CPACK_GENERATOR TGZ)
    endif()
    include(CPack)
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
