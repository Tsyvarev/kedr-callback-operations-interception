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
    kedr_coi_test_add_prefix("indirect_use_copy")
    set(indirect_suffix "_use_copy")
    kbuild_add_definitions("-DINDIRECT_CONSTRUCTOR=kedr_coi_interceptor_create_use_copy")
else(DEFINED indirect_use_copy)
    kedr_coi_test_add_prefix("indirect")
    set(indirect_suffix)
    kbuild_add_definitions("-DINDIRECT_CONSTRUCTOR=kedr_coi_interceptor_create")
endif(DEFINED indirect_use_copy)

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
