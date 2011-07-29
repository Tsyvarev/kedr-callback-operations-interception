set(module_name "file_access_counter_module")

# List of example files which are not configured and should be copied as is
set(sources_non_configured
    "file_access_counter.c"
    "functions.data"
    "file_system_type.data"
    "super_operations.data"
    "inode_operations.data"
    "inode_file_operations.data"
    "dentry_operations.data"
)

kedr_coi_load_install_prefixes()
set(KEDR_COI_INCLUDE_DIR "${KEDR_COI_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(kedr_coi_core_symbols "${KEDR_COI_INSTALL_PREFIX_KSYMVERS}/kedr_coi.symvers")
set(kedr_gen_tool "${KEDR_GEN_TOOL_INSTALLED}")


configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
    @ONLY
)

foreach(source ${sources_non_configured})
    list(APPEND sources_non_configured_abs "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
endforeach(source ${sources_non_configured})


install(FILES   ${sources_non_configured_abs}
        DESTINATION "${KEDR_COI_EXAMPLE_PATH}/file_access_counter"
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
        DESTINATION "${KEDR_COI_EXAMPLE_PATH}/file_access_counter"
        RENAME makefile
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
        DESTINATION "${KEDR_COI_EXAMPLE_PATH}/file_access_counter"
        RENAME Kbuild
)

kedr_coi_test_add_subdirectory(tests)