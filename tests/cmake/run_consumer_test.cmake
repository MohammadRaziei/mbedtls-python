# cmake -DMODE=python|prefix -DPYTHON_INSTALL_DIR=... -DMAIN_BINARY_DIR=... -DEXAMPLE_SOURCE_DIR=...
#       -DEXAMPLE_BINARY_DIR=... -DEXAMPLE_EXE=app|consumer -P run_consumer_test.cmake
foreach(var MODE PYTHON_INSTALL_DIR MAIN_BINARY_DIR EXAMPLE_SOURCE_DIR EXAMPLE_BINARY_DIR EXAMPLE_EXE)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "run_consumer_test.cmake: ${var} not set")
    endif()
endforeach()

file(REMOVE_RECURSE "${EXAMPLE_BINARY_DIR}")

if(MODE STREQUAL "python")
    # Same pattern as nanobind/pybind11: the installed package says where its CMake config lives.
    set(_env ${CMAKE_COMMAND} -E env "PYTHONPATH=${PYTHON_INSTALL_DIR}")
    set(_configure_args)
elseif(MODE STREQUAL "prefix")
    set(_prefix "${EXAMPLE_BINARY_DIR}-prefix")
    file(REMOVE_RECURSE "${_prefix}")
    execute_process(COMMAND ${CMAKE_COMMAND} --install "${MAIN_BINARY_DIR}" --prefix "${_prefix}" --config Release
                    RESULT_VARIABLE install_result OUTPUT_VARIABLE install_output ERROR_VARIABLE install_output)
    if(NOT install_result EQUAL 0)
        message(FATAL_ERROR "cmake --install failed:\n${install_output}")
    endif()
    set(_env ${CMAKE_COMMAND} -E env)
    set(_configure_args -DCMAKE_PREFIX_PATH=${_prefix})
else()
    message(FATAL_ERROR "run_consumer_test.cmake: MODE must be python or prefix")
endif()

execute_process(
    COMMAND ${_env} ${CMAKE_COMMAND} -S "${EXAMPLE_SOURCE_DIR}" -B "${EXAMPLE_BINARY_DIR}"
            -DCMAKE_BUILD_TYPE=Release ${_configure_args}
    RESULT_VARIABLE configure_result OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_output)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "consumer project failed to configure:\n${configure_output}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build "${EXAMPLE_BINARY_DIR}" --config Release
    RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_output)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "consumer project failed to build:\n${build_output}")
endif()

execute_process(
    COMMAND "${EXAMPLE_BINARY_DIR}/${EXAMPLE_EXE}"
    RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_output)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "consumer failed to run:\n${run_output}")
endif()
# SHA-256("abc")
if(NOT run_output MATCHES "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    message(FATAL_ERROR "consumer produced unexpected output:\n${run_output}")
endif()
message(STATUS "OK (${MODE}): ${run_output}")
