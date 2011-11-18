set(module_name "test_instrumentor_factory${indirect_suffix}_no_payloads")

kedr_coi_test_add_kernel("no_payloads"
    ${module_name}
    "test.c"
)