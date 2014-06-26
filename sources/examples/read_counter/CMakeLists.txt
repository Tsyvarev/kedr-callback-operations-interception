set(example_name "read_counter")
# Name of the module created in example
set(module_name "kedr_sample_target")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
    @ONLY
)

add_example(${example_name}
    "makefile"
    "Kbuild"
    "read_counter.c"
    "read_counter.h"
    "cfake.c"
    "cfake.h"
    "cdev_file_operations.yaml"
    "kedr_sample_target"
    "README"
)

# Install example and export it for tests.
install_example(TARGETS ${example_name} EXPORT read_counter_imported
    DESTINATION "${KEDR_COI_INSTALL_PREFIX_EXAMPLES}/read_counter"
)

#install(FILES "kedr_sample_target"
#        DESTINATION "${install_dir}"
#        PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
#)

