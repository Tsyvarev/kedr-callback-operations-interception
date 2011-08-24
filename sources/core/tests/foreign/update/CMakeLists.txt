set(module_name "test_instrumentor_foreign_update")

kedr_coi_test_add_kernel("update"
    ${module_name}
    "test.c"
)