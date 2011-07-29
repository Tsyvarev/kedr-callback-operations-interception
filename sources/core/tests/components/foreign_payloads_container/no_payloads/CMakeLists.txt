set(module_name "test_foreign_payloads_container_no_payloads")

kbuild_include_directories("${foreign_payloads_container_test_common_dir}")

kedr_coi_test_add_kernel("no_payloads"
    ${module_name}
    "test.c"
    "foreign_payloads_container_test_common.c"
    ${foreign_payloads_container_sources}
)

rule_copy_source("${core_dir}"
    ${foreign_payloads_container_sources}
)

rule_copy_source("${foreign_payloads_container_test_common_dir}"
    "foreign_payloads_container_test_common.c"
)