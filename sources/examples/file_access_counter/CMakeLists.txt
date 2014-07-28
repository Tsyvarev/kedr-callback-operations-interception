set(example_name "file_access_counter")
# Name of the module created 
set(module_name "file_access_counter")


configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
    @ONLY
)

example_add(${example_name}
    "makefile" SOURCE "example_makefile"
    "Kbuild"
    "module.c"
    "fs_interception.c"
    "fs_interception.h"
    "functions.yaml"
    "README"
)

# Install example and export it for tests.
example_install(TARGETS ${example_name} EXPORT file_access_counter_imported
    COMPONENT "devel"
    DESTINATION "${KEDR_COI_INSTALL_PREFIX_EXAMPLES}/file_access_counter"
)
