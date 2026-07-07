if(NOT DEFINED TEXTFX_SOURCE_DIR)
    message(FATAL_ERROR "TEXTFX_SOURCE_DIR is required")
endif()

set(application_dir "${TEXTFX_SOURCE_DIR}/src/application")
file(GLOB_RECURSE application_sources
    LIST_DIRECTORIES false
    "${application_dir}/*.cpp"
    "${application_dir}/*.h"
    "${application_dir}/*.hpp"
)

set(found_violation OFF)
foreach(source_file IN LISTS application_sources)
    file(STRINGS "${source_file}" forbidden_includes
        REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]([.][.]/)*(app|infrastructure|render)/"
    )
    foreach(include_line IN LISTS forbidden_includes)
        file(RELATIVE_PATH relative_file "${TEXTFX_SOURCE_DIR}" "${source_file}")
        message(SEND_ERROR "${relative_file}: forbidden application-layer include: ${include_line}")
        set(found_violation ON)
    endforeach()
endforeach()

if(found_violation)
    message(FATAL_ERROR "src/application must not include app/, infrastructure/, or render/ headers. Use an application-owned port/DTO, or keep UI/infrastructure composition in the app layer.")
endif()
