set(module_name "test_instrumentor_indirect${indirect_suffix}_external_null")

kedr_coi_test_add_kernel("external_interception_of_null"
    ${module_name}
    "test.c"
)