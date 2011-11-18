kedr_coi_test_add_prefix("core")
set(core_dir "${CMAKE_CURRENT_SOURCE_DIR}/..")

kedr_coi_test_add_kernel_dependency("${CMAKE_CURRENT_BINARY_DIR}/../kedr_coi.ko")
kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../Module.symvers")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(indirect)
add_subdirectory(direct)
add_subdirectory(factory)
add_subdirectory(creation_interceptor)

set(indirect_use_copy "yes")

add_subdirectory(indirect indirect_use_copy)
add_subdirectory(factory factory_use_copy)
add_subdirectory(creation_interceptor creation_interceptor_use_copy)
