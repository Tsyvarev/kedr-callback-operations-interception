set(module_name "test_instrumentor_indirect${indirect_suffix}_several_payloads")

kedr_coi_test_add_kernel("several_payloads"
    ${module_name}
    "test.c"
)