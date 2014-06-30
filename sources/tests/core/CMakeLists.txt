if(USER_PART)
    # Aggregate prefix for test.
    #
    # It is modified in the subdirectories.
    set(test_name_prefix "core.")
endif(USER_PART)



if(KERNEL_PART)
    kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

    set(__test_harness_header "${CMAKE_CURRENT_SOURCE_DIR}/test_harness.h")
    set(__test_harness_header_factory "${CMAKE_CURRENT_SOURCE_DIR}/test_harness_factory.h")
    set(__test_harness_header_creation "${CMAKE_CURRENT_SOURCE_DIR}/test_harness_creation.h")
endif(KERNEL_PART)


# Add test for interceptor.
# By default, test use only "test_harness.h" header.
function(add_test_interceptor test_name module_name source)
    itesting_path(this_install_dir)

    # Per-kernel directory for test module.
    set(test_module_kernel_install_dir "${this_install_dir}/%kernel%")
    
    if(KERNEL_PART)
        kedr_coi_test_add_kmodule(${module_name} ${source} ${ARGN} ${__test_harness_header})
        
        kbuild_link_module(${module_name} "kedr_coi")
        
        kernel_part_path(test_module_install_dir "${test_module_kernel_install_dir}")
        kbuild_install(TARGETS ${module_name}
            MODULE DESTINATION ${test_module_install_dir}
            COMPONENT "tests-kernel"
        )
    endif(KERNEL_PART)
    if(USER_PART)
        kedr_coi_test_add_kernel("${test_name_prefix}${test_name}"
            "${test_module_kernel_install_dir}/${module_name}.ko" # Test module
            "${KEDR_COI_KERNEL_INSTALL_PREFIX_KMODULE}/kedr_coi.ko" # kedr_coi dependency.)
        )
    endif(USER_PART)
endfunction(add_test_interceptor test_name module_name source)

add_subdirectory(indirect)
add_subdirectory(direct)
add_subdirectory(factory)
add_subdirectory(creation_interceptor)

set(indirect_use_copy "TRUE")

add_subdirectory(indirect indirect_use_copy)
add_subdirectory(factory factory_use_copy)
add_subdirectory(creation_interceptor creation_interceptor_use_copy)
