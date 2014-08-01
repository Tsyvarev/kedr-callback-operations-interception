# The path where the examples will be installed
set (KEDR_COI_EXAMPLE_PATH "${KEDR_COI_INSTALL_PREFIX_EXAMPLES}")

add_subdirectory(read_counter)

# This example uses KEDR framework.
if(KEDR_INSTALL_DIR)
    add_subdirectory(file_access_counter)
endif(KEDR_INSTALL_DIR)
