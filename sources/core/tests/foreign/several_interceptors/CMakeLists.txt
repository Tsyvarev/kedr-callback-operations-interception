set(module_name "test_instrumentor_foreign_several_interceptors")

kedr_coi_test_add_kernel("several_interceptors.01"
    ${module_name}
    "test.c"
)