set(module_name "test_instrumentor_creation${indirect_suffix}_internal_replacement")

kedr_coi_test_add_kernel("internal_replacement"
    ${module_name}
    "test.c"
)