kedr_coi_test_add_prefix("core")
set(core_dir "${CMAKE_CURRENT_SOURCE_DIR}/..")

kedr_coi_test_add_kernel_dependency("${CMAKE_CURRENT_BINARY_DIR}/../kedr_coi.ko")
kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../Module.symvers")

# For include "test_harness.h"
kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
# 
set(__test_harness_header "${CMAKE_CURRENT_SOURCE_DIR}/test_harness.h")
# Simiar to kedr_coi_test_add_kernel, but also add dependency on "test_harness.h".
function(add_test_interceptor test_name module_name source)
    kedr_coi_test_add_kernel(${test_name} ${module_name} ${source} ${ARGN} ${__test_harness_header})
endfunction(add_test_interceptor test_name module_name source)

add_subdirectory(indirect)
add_subdirectory(direct)
add_subdirectory(factory)
add_subdirectory(creation_interceptor)

set(indirect_use_copy "yes")

add_subdirectory(indirect indirect_use_copy)
add_subdirectory(factory factory_use_copy)
add_subdirectory(creation_interceptor creation_interceptor_use_copy)
