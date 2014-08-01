# 
# This CMakeLists.txt is used from two places:
#  - usual directory hierarchy
#  - from factory_use_copy directory
#
# 'indirect_use_copy' variable should be defined in the last case.
#
# 'indirect_suffix' is set to '_use_copy' in the last case.
#
# Source code should use INDIRECT_CONSTRUCTOR macro as constructor
# for indirect interceptor for distinguish cases.

if(DEFINED indirect_use_copy)
    set(indirect_suffix "_use_copy")
    if(KERNEL_PART)
        kbuild_add_definitions("-DINDIRECT_CONSTRUCTOR=kedr_coi_interceptor_create_use_copy")
    endif(KERNEL_PART)
else(DEFINED indirect_use_copy)
    set(indirect_suffix)
    if(KERNEL_PART)
        kbuild_add_definitions("-DINDIRECT_CONSTRUCTOR=kedr_coi_interceptor_create_at_place")
    endif(KERNEL_PART)
endif(DEFINED indirect_use_copy)

if(USER_PART)
    set(test_name_prefix "${test_name_prefix}factory${indirect_suffix}.")
    # Notify ictest, that binary and source subdirs differ.
    ictest_add_current_dir()
endif(USER_PART)

# Common way for add test in that category.
# Automatically assign target name for module built and dependence
# on 'test_harness_factory.h'
function(add_test_interceptor_factory test_name source)
    add_test_interceptor(${test_name}
        "test_interceptor_factory${indirect_suffix}_${test_name}"
        ${source}
        ${ARGN}
        ${__test_harness_header_factory}
    )
endfunction(add_test_interceptor_factory test_name source)


add_subdirectory(base)
add_subdirectory(no_payloads)
add_subdirectory(several_interceptors)
add_subdirectory(several_interceptors1)
add_subdirectory(conflicted_interceptors)
add_subdirectory(update)
add_subdirectory(copy_operations)
add_subdirectory(internal_replacement)
add_subdirectory(self_factory)
add_subdirectory(factory_payload)
