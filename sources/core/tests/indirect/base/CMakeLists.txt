set(module_name "test_instrumentor_indirect${indirect_suffix}_base")

kedr_coi_test_add_kernel("base"
    ${module_name}
    "test.c"
)