# Add test applications.
foreach(_fsp_module IN LISTS FSP_SUPPORTED_MODULES)
    add_subdirectory(
        ${fsp_SOURCE_DIR}/projects/${FSP_PROJECTS_SUBDIR}/${_fsp_module}
        ${CMAKE_CURRENT_BINARY_DIR}/${_fsp_module}/${FSP_BOARD}/gcc
    )
endforeach()

