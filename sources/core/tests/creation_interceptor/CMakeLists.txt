# 
# This CMakeLists.txt is used from two places:
#  - usual directory hierarchy
#  - from creation_use_copy directory
#
# 'indirect_use_copy' variable should be defined in the last case.
#
# 'indirect_suffix' is set to '_use_copy' in the last case.
#
# Source code should use INDIRECT_CONSTRUCTOR macro as constructor
# for indirect interceptor for distinguish cases.

if(DEFINED indirect_use_copy)
    kedr_coi_test_add_prefix("creation_use_copy")
    set(indirect_suffix "_use_copy")
    kbuild_add_definitions("-DINDIRECT_CONSTRUCTOR=kedr_coi_interceptor_create_use_copy")
else(DEFINED indirect_use_copy)
    kedr_coi_test_add_prefix("creation")
    set(indirect_suffix)
    kbuild_add_definitions("-DINDIRECT_CONSTRUCTOR=kedr_coi_interceptor_create")
endif(DEFINED indirect_use_copy)

# For include 'test_harness_creation.h'
kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
set(__test_harness_creation_header "${CMAKE_CURRENT_SOURCE_DIR}/test_harness_creation.h")

# Common way for add test in that category.
# Automatically assign target name for module built and dependence
# on 'test_harness_factory.h'
function(add_test_interceptor_creation test_name source)
    add_test_interceptor(${test_name}
        "test_interceptor_creation${indirect_suffix}_${test_name}"
        ${source}
        ${ARGN}
        ${__test_harness_creation_header}
    )
endfunction(add_test_interceptor_creation test_name source)


add_subdirectory(base)
add_subdirectory(no_payloads)
add_subdirectory(several_interceptors)
add_subdirectory(several_interceptors1)
add_subdirectory(conflicted_interceptors)
add_subdirectory(update)
add_subdirectory(copy_operations)
add_subdirectory(internal_replacement)

add_subdirectory(self_creation)

add_subdirectory(same_tie)

add_subdirectory(null_ops)
