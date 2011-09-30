set(module_name "test_instrumentor_direct_internal_interception")

kedr_coi_test_add_kernel("internal_interception"
    ${module_name}
    "test.c"
)