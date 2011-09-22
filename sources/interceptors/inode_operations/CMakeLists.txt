set(datafile "interceptor.data")

set(pieces "interceptor_common.data")

set(pieces_full)
foreach(piece ${pieces})
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/${piece}")
endforeach(piece ${pieces})

set(is_simple_setattr_exist_message "Looking for simple_setattr() function")
if(NOT DEFINED is_simple_setattr_exist)
    
    kmodule_try_compile(is_simple_setattr_exist_tmp
        "${CMAKE_CURRENT_BINARY_DIR}/try_simple_setattr"
        "${CMAKE_CURRENT_SOURCE_DIR}/check_simple_setattr.c"
    )

    set(is_simple_setattr_exist ${is_simple_setattr_exist_tmp} CACHE
        INTERNAL "Whether simple_setattr function exist.")
else(NOT DEFINED is_simple_setattr_exist)
    set(is_simple_setattr_exist_message "${is_simple_setattr_exist_message} [cached]")
endif(NOT DEFINED is_simple_setattr_exist)

if(is_simple_setattr_exist)
    set(is_simple_setattr_exist_message "${is_simple_setattr_exist_message} - found")
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/setattr_via_simple_setattr.data")
else(is_simple_setattr_exist)
    set(is_simple_setattr_exist_message "${is_simple_setattr_exist_message} - not found")
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/setattr_via_inode_setattr.data")
endif(is_simple_setattr_exist)
message(STATUS "${is_simple_setattr_exist_message}")


kmodule_is_operation_exist(is_fallocate_exist
    "${CMAKE_CURRENT_BINARY_DIR}/try_fallocate"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct inode_operations"
    "fallocate"
)

if(is_fallocate_exist)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/fallocate.data")
endif(is_fallocate_exist)

execute_process(COMMAND cat ${pieces_full}
    RESULT_VARIABLE join_result
    OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${datafile}"
    ERROR_VARIABLE join_error
)

if(join_result)
    message("Error: ${join_error}")
    message(FATAL_ERROR "Failed to join pieces of datafile.")
endif(join_result)

create_predefined_interceptor_header("inode_operations_interceptor.h"
	"${CMAKE_CURRENT_BINARY_DIR}/${datafile}")

create_predefined_interceptor_source("inode_operations_interceptor.c"
	"${CMAKE_CURRENT_BINARY_DIR}/${datafile}")