set(module_name "test_instrumentor_creation${indirect_suffix}_same_tie")

kedr_coi_test_add_kernel("same_tie"
    ${module_name}
    "test.c"
)