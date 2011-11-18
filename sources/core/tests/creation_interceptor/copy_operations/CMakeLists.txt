set(module_name "test_instrumentor_creation${indirect_suffix}_copy_operations")

kedr_coi_test_add_kernel("copy_operations"
    ${module_name}
    "test.c"
)