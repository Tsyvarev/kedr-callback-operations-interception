set(module_name "test_instrumentor_foreign_conflicted_interceptors")

kedr_coi_test_add_kernel("conflicted_interceptors"
    ${module_name}
    "test.c"
)