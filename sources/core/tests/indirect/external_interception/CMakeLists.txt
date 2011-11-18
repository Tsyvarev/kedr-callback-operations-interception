set(module_name "test_instrumentor_indirect${indirect_suffix}_external_interception")

kedr_coi_test_add_kernel("external_interception"
    ${module_name}
    "test.c"
)