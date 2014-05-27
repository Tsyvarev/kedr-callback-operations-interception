set(example_name "file_access_counter")
set(module_name "file_access_counter_module")

kedr_coi_load_install_prefixes()
set(KEDR_COI_INCLUDE_DIR "${KEDR_COI_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_coi_core_symbols "${KEDR_COI_INSTALL_PREFIX_KSYMVERS}/kedr_coi.symvers")
set(jy_tool "${JY_TOOL_INSTALLED}")


configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
    @ONLY
)

add_example(${example_name}
    "makefile"
    "Kbuild"
    "file_access_counter.c"
    "fs_interception.c"
    "fs_interception.h"
    "functions.yaml"
    "README"
)

install_example(${example_name} "file_access_counter")

kedr_coi_test_add_subdirectory(tests)
