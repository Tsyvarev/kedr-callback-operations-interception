set(example_name "file_access_counter")
set(module_name "file_access_counter_module")

# Example itself is one-per-all kernels.
# But it is tested in kernel-dependent manner, because it should
# be compiled during tests.
# Kernel and user parts are interoperate using export(TARGET) mechanism.
set(export_file "${CMAKE_CURRENT_BINARY_DIR}/export_example.cmake")
user_build_path(export_file)

if(USER_PART)
    kedr_coi_load_install_paths()
    set(KEDR_COI_INCLUDE_DIR "${KEDR_COI_INSTALL_INCLUDE_DIR}")
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
        "file_access_counter.c"
        "fs_interception.c"
        "fs_interception.h"
        "functions.yaml"
        "README"
    )

    install_example(${example_name} "file_access_counter")
    if(NOT KERNEL_PART)
        example_export(TARGETS ${example_name} FILE "${export_file}"
        )
    endif(NOT KERNEL_PART)
endif(USER_PART)
if(KERNEL_PART)
    # import
    if(NOT USER_PART)
        include("${export_file}")
    endif(NOT USER_PART)
    kedr_coi_test_add_subdirectory(tests)
endif(KERNEL_PART)
