set(datafile "interceptor.data")

set(pieces "interceptor_common.data")

set(pieces_full)
foreach(piece ${pieces})
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/${piece}")
endforeach(piece ${pieces})

kmodule_is_operation_exist(is_get_sb_exist
    "${CMAKE_CURRENT_BINARY_DIR}/get_sb"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct file_system_type"
    "get_sb"
)

if(is_get_sb_exist)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/get_sb.data")
else(is_get_sb_exist)
    kmodule_is_operation_exist(is_mount_exist
        "${CMAKE_CURRENT_BINARY_DIR}/mount"
        ${CMAKE_CURRENT_SOURCE_DIR}
        "struct file_system_type"
        "mount"
    )
    if(NOT is_mount_exist)
        message(FATAL_ERROR "Neither 'get_sb' nor 'mount' methods are defined for 'struct file_system_type'. It is unexpected situation.")
    endif(NOT is_mount_exist)
    
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/mount.data")
endif(is_get_sb_exist)

execute_process(COMMAND cat ${pieces_full}
    RESULT_VARIABLE join_result
    OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${datafile}"
    ERROR_VARIABLE join_error
)

if(join_result)
    message("Error: ${join_error}")
    message(FATAL_ERROR "Failed to join pieces of datafile.")
endif(join_result)

create_predefined_interceptor_header("file_system_type_interceptor.h"
	"${CMAKE_CURRENT_BINARY_DIR}/${datafile}")

create_predefined_interceptor_source("file_system_type_interceptor.c"
	"${CMAKE_CURRENT_BINARY_DIR}/${datafile}")