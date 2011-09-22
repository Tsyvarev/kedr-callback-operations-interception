set(datafile "interceptor.data")

set(pieces "interceptor_common.data")

set(pieces_full)
foreach(piece ${pieces})
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/${piece}")
endforeach(piece ${pieces})

kmodule_is_operation_exist(is_dirty_inode_accept_flags
    "${CMAKE_CURRENT_BINARY_DIR}/try_dirty_inode_with_flags"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct super_operations"
    "dirty_inode"
    "void (*)(struct inode*, int flags)"
)

if(is_dirty_inode_accept_flags)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/dirty_inode_with_flags.data")
else(is_dirty_inode_accept_flags)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/dirty_inode.data")
endif(is_dirty_inode_accept_flags)


kmodule_is_operation_exist(is_write_inode_accept_wbc
    "${CMAKE_CURRENT_BINARY_DIR}/try_write_inode_with_wbc"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct super_operations"
    "write_inode"
    "int (*)(struct inode*, struct writeback_control* wbc)"
)

if(is_write_inode_accept_wbc)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/write_inode_wbc.data")
else(is_write_inode_accept_wbc)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/write_inode.data")
endif(is_write_inode_accept_wbc)


kmodule_is_operation_exist(is_drop_inode_return_int
    "${CMAKE_CURRENT_BINARY_DIR}/try_drop_inode_return_int"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct super_operations"
    "drop_inode"
    "int (*)(struct inode*)"
)

if(is_drop_inode_return_int)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/drop_inode_return_int.data")
else(is_drop_inode_return_int)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/drop_inode.data")
endif(is_drop_inode_return_int)


kmodule_is_operation_exist(is_delete_inode_exist
    "${CMAKE_CURRENT_BINARY_DIR}/try_delete_inode"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct super_operations"
    "delete_inode"
)

if(is_delete_inode_exist)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/delete_inode.data")
endif(is_delete_inode_exist)


kmodule_is_operation_exist(is_clear_inode_exist
    "${CMAKE_CURRENT_BINARY_DIR}/try_clear_inode"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct super_operations"
    "clear_inode"
)

if(is_clear_inode_exist)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/clear_inode.data")
endif(is_clear_inode_exist)


kmodule_is_operation_exist(is_show_devname_exist
    "${CMAKE_CURRENT_BINARY_DIR}/try_show_devname"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct super_operations"
    "show_devname"
)

if(is_show_devname_exist)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/show_devname.data")
endif(is_show_devname_exist)


kmodule_is_operation_exist(is_show_path_exist
    "${CMAKE_CURRENT_BINARY_DIR}/try_show_path"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct super_operations"
    "show_path"
)

if(is_show_path_exist)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/show_path.data")
endif(is_show_path_exist)


execute_process(COMMAND cat ${pieces_full}
    RESULT_VARIABLE join_result
    OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${datafile}"
    ERROR_VARIABLE join_error
)

if(join_result)
    message("Error: ${join_error}")
    message(FATAL_ERROR "Failed to join pieces of datafile.")
endif(join_result)

create_predefined_interceptor_header("super_operations_interceptor.h"
	"${CMAKE_CURRENT_BINARY_DIR}/${datafile}")

create_predefined_interceptor_source("super_operations_interceptor.c"
	"${CMAKE_CURRENT_BINARY_DIR}/${datafile}")