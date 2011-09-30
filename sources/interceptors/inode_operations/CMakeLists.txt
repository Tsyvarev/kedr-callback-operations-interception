set(datafile "interceptor.data")

set(pieces "interceptor_common.data")

set(pieces_full)
foreach(piece ${pieces})
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/${piece}")
endforeach(piece ${pieces})

kmodule_is_operation_exist(is_inode_fallocate_exist
    "${CMAKE_CURRENT_BINARY_DIR}/try_fallocate"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct inode_operations"
    "fallocate"
)

if(is_inode_fallocate_exist)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/fallocate.data")
endif(is_inode_fallocate_exist)

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