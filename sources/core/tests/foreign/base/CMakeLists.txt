set(module_name "test_instrumentor_foreign_base")

kedr_coi_test_add_kernel("base"
    ${module_name}
    "test.c"
)