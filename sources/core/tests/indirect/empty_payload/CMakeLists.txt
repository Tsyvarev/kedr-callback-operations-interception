set(module_name "test_instrumentor_indirect_empty_payload")

kedr_coi_test_add_kernel("empty_payload"
    ${module_name}
    "test.c"
)