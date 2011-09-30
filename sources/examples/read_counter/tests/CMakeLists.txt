configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY)

add_custom_target("read_counter_module_test"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/kedr_sample_target"
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
    COMMAND $(MAKE) -f makefile_test
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
            "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
            ${sources_non_configured}
)

kedr_coi_load_test_prefixes()
set(KEDR_COI_INCLUDE_DIR "${KEDR_COI_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_coi_core_symbols "${CMAKE_BINARY_DIR}/core/Module.symvers")
set(kedr_gen_tool "${KEDR_GEN_TOOL}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

rule_copy_source("${CMAKE_CURRENT_SOURCE_DIR}/.."
    ${sources_non_configured} "kedr_sample_target"
)

kedr_coi_test_add_target("read_counter_module_test")

kedr_coi_test_add_script("read_counter" "test.sh")