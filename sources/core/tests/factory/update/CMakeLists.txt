set(module_name "test_instrumentor_factory${indirect_suffix}_update")

kedr_coi_test_add_kernel("update"
    ${module_name}
    "test.c"
)