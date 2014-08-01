kmodule_configure_operations_setup("inode_operations"
    "${CMAKE_CURRENT_SOURCE_DIR}/operations_include.h"
    "struct inode_operations")

kmodule_configure_operations(operations_list
    "link"
    "unlink"
    "symlink"
    "rmdir"
    "permission"
    "rename"
    "readlink"
    "follow_link"
    "put_link"
    "setattr"
    "getattr"
    "setxattr"
    "getxattr"
    "listxattr"
    "removexattr"
    "fiemap"
OPTIONAL
    "fallocate"
    "truncate"
    "truncate_range"
    "get_acl"
REQUIRED
    "create"
        TYPED "accept_bool" "int (*)(struct inode*, struct dentry*, umode_t, bool)"
        TYPED "accept_umode" "int (*)(struct inode*, struct dentry*, umode_t, struct nameidata*)"
        TYPED "accept_int_nd" "int (*)(struct inode*, struct dentry*, int, struct nameidata*)"
    "lookup"
        TYPED "accept_uint" "struct dentry* (*)(struct inode*, struct dentry*, unsigned int)"
        TYPED "accept_nd" "struct dentry* (*)(struct inode*, struct dentry*, struct nameidata*)"
    "mkdir"
        TYPED "accept_umode" "int (*)(struct inode*, struct dentry*, umode_t)"
        TYPED "accept_int" "int (*)(struct inode*, struct dentry*, int)"
    "mknod"
        TYPED "accept_umode" "int (*)(struct inode*, struct dentry*, umode_t, dev_t)"
        TYPED "accept_int" "int (*)(struct inode*, struct dentry*, int, dev_t)"
)


add_interceptor("inode_operations_interceptor" "header.yaml" ${operations_list})
