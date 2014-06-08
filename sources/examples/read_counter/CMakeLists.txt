set(example_name "read_counter")
set(module_name "kedr_sample_target")

# Example itself is one-per-all kernels.
# But it is tested in kernel-dependent manner, because it should
# be compiled during tests.
# Kernel and user parts are interoperate using export(TARGET) mechanism.

set(export_file "${CMAKE_CURRENT_BINARY_DIR}/export_example.cmake")
user_build_path(export_file)

if(USER_PART)
    kedr_coi_load_install_paths()
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

    install_example(${example_name} "read_counter")

    #install(FILES "kedr_sample_target"
    #        DESTINATION "${install_dir}"
    #        PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
    #)

    # export
    if(NOT KERNEL_PART)
        example_export(TARGETS ${example_name} FILE "${export_file}")
    endif(NOT KERNEL_PART)
endif(USER_PART)

if(KERNEL_PART)
    # import
    if(NOT USER_PART)
        include("${export_file}")
    endif(NOT USER_PART)

    kedr_coi_test_add_subdirectory(tests)
endif(KERNEL_PART)
