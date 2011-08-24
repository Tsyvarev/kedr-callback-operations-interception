set(module_name "test_instrumentor_indirect_base")

kedr_coi_test_add_kernel("base"
    ${module_name}
    "test.c"
)