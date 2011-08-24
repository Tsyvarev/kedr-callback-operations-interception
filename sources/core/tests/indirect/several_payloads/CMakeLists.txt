set(module_name "test_instrumentor_indirect_several_payloads")

kedr_coi_test_add_kernel("several_payloads"
    ${module_name}
    "test.c"
)