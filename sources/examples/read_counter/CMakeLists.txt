set(module_name "read_counters_module")

kedr_coi_load_install_prefixes()
set(KEDR_COI_INCLUDE_DIR "${KEDR_COI_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/read_counter.conf.in"
    "${CMAKE_CURRENT_BINARY_DIR}/read_counter.conf"
    @ONLY
)


set(kedr_coi_core_symbols "${KEDR_COI_INSTALL_PREFIX_KSYMVERS}/kedr_coi.symvers")
set(kedr_gen_tool "${KEDR_GEN_TOOL_INSTALLED}")


configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
    @ONLY
)


install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/read_counter.c"
                "${CMAKE_CURRENT_SOURCE_DIR}/functions.data"
                "${CMAKE_CURRENT_SOURCE_DIR}/file_operations.data"
                "${CMAKE_CURRENT_SOURCE_DIR}/cdev_file_operations.data"
        DESTINATION "${KEDR_COI_EXAMPLE_PATH}/read_counter"
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
        DESTINATION "${KEDR_COI_EXAMPLE_PATH}/read_counter"
        RENAME makefile
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
        DESTINATION "${KEDR_COI_EXAMPLE_PATH}/read_counter"
        RENAME Kbuild
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/read_counter.conf"
        DESTINATION "${KEDR_COI_EXAMPLE_PATH}/read_counter"
        RENAME Kbuild
)


kedr_coi_test_add_subdirectory(tests)