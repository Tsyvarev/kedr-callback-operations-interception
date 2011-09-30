set(module_name "test_instrumentor_foreign_bind_with_internal_replacement")

kedr_coi_test_add_kernel("bind_with_internal_replacement"
    ${module_name}
    "test.c"
)