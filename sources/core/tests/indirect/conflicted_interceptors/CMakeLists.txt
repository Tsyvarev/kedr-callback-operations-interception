set(module_name "test_instrumentor_indirect${indirect_suffix}_conflicted_interceptors")

kedr_coi_test_add_kernel("conflicted_interceptors"
    ${module_name}
    "test.c"
)