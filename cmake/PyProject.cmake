# PyProject.cmake: reads name, version and description from the [project] table of pyproject.toml, so the Python
# package and CMake share one version, the `version` of pyproject.toml (idea and PyProject_* names: liburlparser).
# Sets PyProject_NAME, PyProject_VERSION, PyProject_DESCRIPTION and PyProject_TOML_PATH in the caller's scope.
#
# ponytail: a scanner, not a TOML parser. It understands `key = "value"` on one line at the start of a line inside
# [project], which is all pyproject.toml needs for these three keys (a same-named key in another table, or an indented
# `{ name = ... }` inside an array, is not picked up). Upgrade path: a real TOML reader if a key ever needs more.
function(extract_pyproject_info)
    set(filename "${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml")
    set(PyProject_TOML_PATH "${filename}" PARENT_SCOPE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${filename}")  # re-run CMake when the version changes

    file(READ "${filename}" toml)
    set(toml "\n${toml}")
    string(FIND "${toml}" "\n[project]\n" start)
    if(start EQUAL -1)
        message(FATAL_ERROR "PyProject: no [project] table in ${filename}")
    endif()
    string(SUBSTRING "${toml}" ${start} -1 block)
    string(SUBSTRING "${block}" 10 -1 block)  # drop the "\n[project]" header, keep the newline after it
    string(FIND "${block}" "\n[" next)        # the next table starts the next line with "["
    if(NOT next EQUAL -1)
        string(SUBSTRING "${block}" 0 ${next} block)
    endif()

    foreach(key name version description)
        if("${block}" MATCHES "\n${key}[ \t]*=[ \t]*\"([^\"\n]*)\"")
            string(TOUPPER ${key} upper)
            set(PyProject_${upper} "${CMAKE_MATCH_1}")
            set(PyProject_${upper} "${CMAKE_MATCH_1}" PARENT_SCOPE)
        endif()
    endforeach()
    if(NOT PyProject_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "PyProject: need a literal version = \"X.Y.Z\" in [project] of ${filename} "
                            "(found '${PyProject_VERSION}'); a dynamic version cannot be read from here")
    endif()
endfunction()

extract_pyproject_info()
