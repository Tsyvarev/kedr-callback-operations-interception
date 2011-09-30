kedr_coi_test_add_prefix("foreign")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(base)
add_subdirectory(no_payloads)
add_subdirectory(several_interceptors)
add_subdirectory(several_interceptors1)
add_subdirectory(conflicted_interceptors)
add_subdirectory(update)
add_subdirectory(copy_operations)
add_subdirectory(bind_with_internal_replacement)
