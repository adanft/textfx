if(NOT DEFINED TEXTFX_SOURCE_DIR)
    message(FATAL_ERROR "TEXTFX_SOURCE_DIR is required")
endif()

function(textfx_check_forbidden_layer_includes layer_name forbidden_layers)
    set(layer_dir "${TEXTFX_SOURCE_DIR}/src/${layer_name}")
    file(GLOB_RECURSE layer_sources
        LIST_DIRECTORIES false
        "${layer_dir}/*.cpp"
        "${layer_dir}/*.h"
        "${layer_dir}/*.hpp"
    )

    foreach(source_file IN LISTS layer_sources)
        file(STRINGS "${source_file}" forbidden_includes
            REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]([.][.]/)*(${forbidden_layers})/"
        )
        foreach(include_line IN LISTS forbidden_includes)
            file(RELATIVE_PATH relative_file "${TEXTFX_SOURCE_DIR}" "${source_file}")
            message(SEND_ERROR "${relative_file}: forbidden ${layer_name}-layer include: ${include_line}")
            set(TEXTFX_FOUND_LAYER_VIOLATION ON PARENT_SCOPE)
        endforeach()
    endforeach()
endfunction()

set(TEXTFX_FOUND_LAYER_VIOLATION OFF)

textfx_check_forbidden_layer_includes("domain" "app|application|infrastructure|render")
textfx_check_forbidden_layer_includes("application" "app|infrastructure|render")
textfx_check_forbidden_layer_includes("infrastructure" "app|render")
textfx_check_forbidden_layer_includes("render" "app")

if(TEXTFX_FOUND_LAYER_VIOLATION)
    message(FATAL_ERROR "Architecture boundary violation. Approved dependency flow is domain <- application <- infrastructure <- render <- app; upper layers may use lower-layer ports/adapters, but lower layers must not include upper layers.")
endif()
