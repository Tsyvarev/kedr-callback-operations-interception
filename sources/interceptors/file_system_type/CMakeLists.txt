kmodule_configure_operations_setup("file_system_type"
    "${CMAKE_CURRENT_SOURCE_DIR}/operations_include.h"
    "struct file_system_type"
)

kmodule_configure_operations(operations_list
    "kill_sb"
)

# "On-of"
kmodule_configure_operations(get_sb_or_mount
OPTIONAL
    "mount"
    "get_sb"
)

list(LENGTH get_sb_or_mount get_sb_or_mount_n)

if(NOT get_sb_or_mount_n EQUAL 1)
    message(FATAL_ERROR "'get_sb' or 'mount' operation should be defined for struct file_system_type.")
endif(NOT get_sb_or_mount_n EQUAL 1)

add_interceptor("file_system_type_interceptor"
    "header.yaml"
    ${operations_list}
    ${get_sb_or_mount})
