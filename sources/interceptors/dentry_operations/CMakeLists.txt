kmodule_configure_operations_setup("dentry_operations"
    "${CMAKE_CURRENT_SOURCE_DIR}/operations_include.h"
    "struct dentry_operations"
)

kmodule_configure_operations(operations_list
    "d_release"
    "d_iput"
    "d_dname"
REQUIRED
    "d_revalidate"
        TYPED "accept_uint" "int (*)(struct dentry*, unsigned int)"
        TYPED "accept_nd" "int (*)(struct dentry*, struct nameidata*)"
    "d_hash"
        TYPED "with_inode" "int (*)(const struct dentry*, const struct inode*, struct qstr*)"
        TYPED "const" "int (*)(const struct dentry*, struct qstr*)"
        TYPED "simple" "int (*)(struct dentry*, struct qstr*)"
    "d_compare"
        TYPED "1qstr" "int (*)(const struct dentry*, const struct dentry*, unsigned int, const char*, const struct qstr*)"
        TYPED "1qstr_with_inode" "int (*)(const struct dentry*, const struct inode*, const struct dentry*, const struct inode*, unsigned int, const char*, const struct qstr*)"
        TYPED "2qstr" "int (*)(struct dentry*, struct qstr*, struct qstr*)"
    "d_delete"
        TYPED "const" "int (*)(const struct dentry*)"
        TYPED "noconst" "int (*)(struct dentry*)"
OPTIONAL
    "d_automount"
    "d_manage"
)

add_interceptor("dentry_operations_interceptor"
    "header.yaml"
    ${operations_list}
)
