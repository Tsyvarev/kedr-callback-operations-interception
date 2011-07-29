kedr_coi_test_add_prefix("foreign_payloads_container")

set(foreign_payloads_container_test_common_dir "${CMAKE_CURRENT_SOURCE_DIR}")

set(foreign_payloads_container_sources
    "kedr_coi_payloads_container.c"
)

add_subdirectory(base)
add_subdirectory(several_payloads)
add_subdirectory(no_payloads)
add_subdirectory(empty_payload)