kmodule_configure_operations_setup("file_operations"
    "${CMAKE_CURRENT_SOURCE_DIR}/operations_include.h"
    "struct file_operations")

kmodule_configure_operations(operations_list
    "llseek"
    "read"
    "write"
    "aio_read"
    "aio_write"
    "poll"
    "unlocked_ioctl"
    "compat_ioctl"
    "mmap"
    "open"
    "flush"
    "release"
    "aio_fsync"
    "lock"
    "sendpage"
    "get_unmapped_area"
    "flock"
    "splice_write"
    "splice_read"
    "setlease"
OPTIONAL
    "ioctl"
    "fallocate"
REQUIRED
    "fsync"
        TYPED "with_dentry" "int (*) (struct file *, struct dentry *, int)"
        TYPED "with_range" "int (*) (struct file *, loff_t, loff_t, int)"
        TYPED "simple" "int (*) (struct file *, int)"
)

# Analogue for ON_OF from KEDR: only one operation should exist.
kmodule_configure_operations(iterate_or_readdir
OPTIONAL
    "iterate"
    "readdir"
)

list(LENGTH iterate_or_readdir iterate_or_readdir_n)
if(NOT iterate_or_readdir_n EQUAL 1)
    message(FATAL_ERROR "One of operations 'iterate' and 'readdir' should exist")
endif(NOT iterate_or_readdir_n EQUAL 1)


add_interceptor("file_operations_interceptor"
    "header.yaml"
    ${operations_list}
    ${iterate_or_readdir}
)
