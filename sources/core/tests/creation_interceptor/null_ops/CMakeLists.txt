set(module_name "test_instrumentor_creation${indirect_suffix}_null_ops")

kedr_coi_test_add_kernel("null_ops"
    ${module_name}
    "test.c"
)