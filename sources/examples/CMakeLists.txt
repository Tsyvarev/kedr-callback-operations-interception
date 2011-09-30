# The path where the examples will be installed
set (KEDR_COI_EXAMPLE_PATH "${KEDR_COI_INSTALL_PREFIX_EXAMPLES}")

set (KEDR_COI_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")
# See comments in include/CMakeLists.txt about headers in examples.
set (KEDR_COI_TEST_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include/install")

add_subdirectory(read_counter)

# This example use KEDR framework.
if(KEDR_INSTALL_DIR)
    add_subdirectory(file_access_counter)
endif(KEDR_INSTALL_DIR)