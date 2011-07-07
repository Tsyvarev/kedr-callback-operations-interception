set(module_name "test_instrumentor_indirect_update")

set(relative_core_dir "../../../..")
set(relative_instrumentor_test_common_dir "../..")

kbuild_include_directories("${instrumentor_test_common_dir}")

kedr_coi_test_add_kernel("update"
    ${module_name}
    "test.c"
    "instrumentor_test_common.c"
    "kedr_coi_instrumentors.c"
    "kedr_coi_global_map.c"
    "kedr_coi_hash_table.c"
)

rule_copy_source("${core_dir}"
    "kedr_coi_instrumentors.c"
    "kedr_coi_global_map.c"
    "kedr_coi_hash_table.c"
)

rule_copy_source("${instrumentor_test_common_dir}"
    "instrumentor_test_common.c"
)