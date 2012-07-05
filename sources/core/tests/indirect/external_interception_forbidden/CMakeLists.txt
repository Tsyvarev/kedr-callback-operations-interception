set(module_name "test_instrumentor_indirect${indirect_suffix}_no_external")

kedr_coi_test_add_kernel("external_interception_forbidden"
    ${module_name}
    "test.c"
)