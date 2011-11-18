set(module_name "test_instrumentor_creation${indirect_suffix}_self_creation")

kedr_coi_test_add_kernel("self_creation"
    ${module_name}
    "test.c"
)