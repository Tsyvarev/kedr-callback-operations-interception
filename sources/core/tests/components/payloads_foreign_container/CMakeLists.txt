kedr_coi_test_add_prefix("payloads_foreign_container")

set(payloads_foreign_container_test_common_dir "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(base)
add_subdirectory(several_payloads)
add_subdirectory(no_payloads)
add_subdirectory(empty_payload)