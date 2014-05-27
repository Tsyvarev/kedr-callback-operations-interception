set(example_name "read_counter")
set(module_name "kedr_sample_target")

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
    "read_counter.c"
    "read_counter.h"
    "cfake.c"
    "cfake.h"
    "cdev_file_operations.yaml"
    "kedr_sample_target"
    "README"
)

install_example(${example_name} "read_counter")


#install(FILES "kedr_sample_target"
#        DESTINATION "${install_dir}"
#        PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
#)

kedr_coi_test_add_subdirectory(tests)
