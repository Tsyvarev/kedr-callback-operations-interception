set(module_name "test_instrumentor_indirect_grouping")

kedr_coi_test_add_kernel("grouping"
    ${module_name}
    "test.c"
)