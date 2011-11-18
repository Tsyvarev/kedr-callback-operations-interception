set(module_name "test_instrumentor_factory${indirect_suffix}_several_interceptors1")

kedr_coi_test_add_kernel("several_interceptors.02"
    ${module_name}
    "test.c"
)