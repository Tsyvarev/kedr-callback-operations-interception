kedr_coi_test_add_prefix("instrumentor")

set(instrumentor_sources
    "kedr_coi_instrumentors.c"
    "kedr_coi_hash_table.c"
)
set(instrumentor_test_common_dir "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(indirect)
add_subdirectory(direct)