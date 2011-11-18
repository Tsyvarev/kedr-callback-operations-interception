set(module_name "test_instrumentor_factory${indirect_suffix}_self_factory")

kedr_coi_test_add_kernel("self_factory"
    ${module_name}
    "test.c"
)