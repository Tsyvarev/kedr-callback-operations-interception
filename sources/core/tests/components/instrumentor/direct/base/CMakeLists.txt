set(module_name "test_instrumentor_direct_base")

kbuild_include_directories("${instrumentor_test_common_dir}")

kedr_coi_test_add_kernel("base"
    ${module_name}
    "test.c"
    "instrumentor_test_common.c"
    ${instrumentor_sources}
)

rule_copy_source("${core_dir}"
    ${instrumentor_sources}
)

rule_copy_source("${instrumentor_test_common_dir}"
    "instrumentor_test_common.c"
)