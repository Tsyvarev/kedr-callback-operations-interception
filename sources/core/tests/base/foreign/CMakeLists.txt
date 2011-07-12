kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")

kedr_coi_test_add_kernel_dependency("${CMAKE_BINARY_DIR}/core/kedr_coi.ko")
kedr_coi_test_add_kernel("foreign"
    "test_interceptor_base_foreign"
    "test.c"
)