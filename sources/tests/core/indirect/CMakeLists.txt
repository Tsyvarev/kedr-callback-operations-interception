# 
# This CMakeLists.txt is used from two places:
#  - usual directory hierarchy
#  - from indirect_use_copy directory
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
    set(test_name_prefix "${test_name_prefix}indirect${indirect_suffix}.")
    # Notify ictest, that binary and source subdirs differ.
    ictest_add_current_dir()
endif(USER_PART)


# Common way for add test in that category.
# Automatically assign target name for module built.
function(add_test_interceptor_indirect test_name source)
    add_test_interceptor(${test_name}
        "test_interceptor_indirect${indirect_suffix}_${test_name}"
        ${source}
        ${ARGN}
    )
endfunction(add_test_interceptor_indirect test_name source)

add_subdirectory(base)
add_subdirectory(several_payloads)
add_subdirectory(no_payloads)
add_subdirectory(empty_payload)
add_subdirectory(grouping)
add_subdirectory(conflicted_interceptors)
add_subdirectory(update)
add_subdirectory(copy_operations)
add_subdirectory(internal_interception)
add_subdirectory(external_interception)
add_subdirectory(external_interception_forbidden)
add_subdirectory(external_interception_null)
