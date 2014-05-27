# Directory where example will be installed for testing.
set(example_dir "example")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY)

kedr_coi_load_test_prefixes()
set(KEDR_COI_INCLUDE_DIR "${KEDR_COI_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_coi_core_symbols "${CMAKE_BINARY_DIR}/core/Module.symvers")
set(jy_tool "${JY_TOOL}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
    @ONLY
)

test_add_example_copy("file_access_counter_test" "file_access_counter" ${example_dir}
    "makefile"
    "Kbuild"
)

kedr_coi_test_add_script("file_access_counter" "test.sh")
