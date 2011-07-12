set(module_name "test_payloads_container_grouping_simple")

kbuild_include_directories("${payloads_container_test_common_dir}")

kedr_coi_test_add_kernel("simple"
    ${module_name}
    "test.c"
    "payloads_container_test_common.c"
    "kedr_coi_payloads_container.c"
)

rule_copy_source("${core_dir}"
    "kedr_coi_payloads_container.c"
)

rule_copy_source("${payloads_container_test_common_dir}"
    "payloads_container_test_common.c"
)