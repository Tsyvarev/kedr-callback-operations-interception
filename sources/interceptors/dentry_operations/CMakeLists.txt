set(datafile "interceptor.data")

set(pieces "interceptor_common.data")

set(pieces_full)
foreach(piece ${pieces})
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/${piece}")
endforeach(piece ${pieces})

kmodule_is_operation_exist(is_d_hash_accept_inode
    "${CMAKE_CURRENT_BINARY_DIR}/try_d_hash_accept_inode"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct dentry_operations"
    "d_hash"
    "int (*)(const struct dentry *, const struct inode *, struct qstr *)"
)

if(is_d_hash_accept_inode)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/d_hash_with_inode.data")
else(is_d_hash_accept_inode)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/d_hash.data")
endif(is_d_hash_accept_inode)


kmodule_is_operation_exist(is_d_compare_accept_inode
    "${CMAKE_CURRENT_BINARY_DIR}/try_d_compare_accept_inode"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct dentry_operations"
    "d_compare"
    "int (*d_compare)(const struct dentry *, const struct inode *, const struct dentry *, const struct inode *, unsigned int, const char *, const struct qstr *);"
)

if(is_d_compare_accept_inode)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/d_compare_with_inode.data")
else(is_d_compare_accept_inode)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/d_compare.data")
endif(is_d_compare_accept_inode)


kmodule_is_operation_exist(is_d_delete_const
    "${CMAKE_CURRENT_BINARY_DIR}/try_d_delete_const"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct dentry_operations"
    "d_delete"
    "int (*)(const struct dentry *)"
)

if(is_d_delete_const)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/d_delete_const.data")
else(is_d_delete_const)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/d_delete.data")
endif(is_d_delete_const)


kmodule_is_operation_exist(is_d_automount_exist
    "${CMAKE_CURRENT_BINARY_DIR}/try_d_automount"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct dentry_operations"
    "d_automount"
)

if(is_d_automount_exist)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/d_automount.data")
endif(is_d_automount_exist)


kmodule_is_operation_exist(is_d_manage_exist
    "${CMAKE_CURRENT_BINARY_DIR}/try_d_manage"
    ${CMAKE_CURRENT_SOURCE_DIR}
    "struct dentry_operations"
    "d_manage"
)

if(is_d_manage_exist)
    list(APPEND pieces_full "${CMAKE_CURRENT_SOURCE_DIR}/d_manage.data")
endif(is_d_manage_exist)


execute_process(COMMAND cat ${pieces_full}
    RESULT_VARIABLE join_result
    OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${datafile}"
    ERROR_VARIABLE join_error
)

if(join_result)
    message("Error: ${join_error}")
    message(FATAL_ERROR "Failed to join pieces of datafile.")
endif(join_result)

create_predefined_interceptor_header("dentry_operations_interceptor.h"
	"${CMAKE_CURRENT_BINARY_DIR}/${datafile}")

create_predefined_interceptor_source("dentry_operations_interceptor.c"
	"${CMAKE_CURRENT_BINARY_DIR}/${datafile}")