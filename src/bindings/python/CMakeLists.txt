# ${SKBUILD_SABI_COMPONENT} is Development.SABIModule when building a stable-ABI wheel (wheel.py-api in
# pyproject.toml) and empty otherwise (free-threaded builds cannot use abi3).
find_package(Python REQUIRED COMPONENTS Interpreter Development.Module ${SKBUILD_SABI_COMPONENT})

if(DEFINED SKBUILD)
    find_package(Cython MODULE REQUIRED VERSION 3.1)
    include(UseCython)

    cython_transpile(${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/_mbedtls_cy.pyx LANGUAGE CXX OUTPUT_VARIABLE _mbedtls_cxx)
    if(NOT "${SKBUILD_SABI_VERSION}" STREQUAL "")
        set(USE_SABI USE_SABI ${SKBUILD_SABI_VERSION})
    endif()
    if(SKBUILD_SOABI)
        set(Python_SOABI ${SKBUILD_SOABI})
    endif()
    python_add_library(_mbedtls_cy MODULE "${_mbedtls_cxx}" ${MBEDTLS_CPP_SHIM_SOURCES} WITH_SOABI ${USE_SABI})
    mbedtls_cpp_apply_shim_build_settings(_mbedtls_cy)
    target_link_libraries(_mbedtls_cy PRIVATE mbedtls mbedx509 tfpsacrypto Threads::Threads)

    # libmbedtls.so, libmbedx509.so and libtfpsacrypto.so (Mbed TLS itself) are installed in mbedtls/sdk/lib, with the cmake
    # folder (and mbedtls/sdk/include beside it): the extension finds them from there. (Windows has no rpath:
    # mbedtls/__init__.py registers that folder with os.add_dll_directory.)
    if(APPLE)
        set_target_properties(_mbedtls_cy PROPERTIES INSTALL_RPATH "@loader_path/sdk/lib")
    elseif(UNIX)
        set_target_properties(_mbedtls_cy PROPERTIES INSTALL_RPATH "$ORIGIN/sdk/lib")
    endif()

    install(TARGETS _mbedtls_cy LIBRARY DESTINATION mbedtls)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/ DESTINATION mbedtls FILES_MATCHING PATTERN "*.py")
    return()
endif()

# Standalone (non-SKBUILD) build: build the extension in-tree via ExternalProject so `cmake --build` + `ctest`
# exercise the same path as `pip install .` (the same sdk layout inside the package), without re-running the
# top-level configure under scikit-build-core.
include(ExternalProject)
ExternalProject_Add(build_python
    SOURCE_DIR ${CMAKE_SOURCE_DIR}
    BINARY_DIR ${CMAKE_BINARY_DIR}/python/build
    INSTALL_DIR ${PYTHON_INSTALL_DIR}
    CMAKE_ARGS
        -DSKBUILD=2
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${PYTHON_INSTALL_DIR}
        -DMBEDTLS_CPP_BUILD_TESTS=OFF
        -DMBEDTLS_CPP_BUILD_PYTHON=ON
        -DMBEDTLS_CPP_THREADING_ALT=${MBEDTLS_CPP_THREADING_ALT}
        -DPython_EXECUTABLE=${Python_EXECUTABLE}
        -DPython3_EXECUTABLE=${Python_EXECUTABLE}
    INSTALL_COMMAND ${CMAKE_COMMAND} --install . --config Release
    BUILD_ALWAYS 1
    USES_TERMINAL_BUILD 1
)
