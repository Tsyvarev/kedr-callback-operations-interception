kmodule_configure_operations_setup("super_operations"
    "${CMAKE_CURRENT_SOURCE_DIR}/operations_include.h"
    "struct super_operations")

kmodule_configure_operations(operations_list
    "alloc_inode"
    "destroy_inode"
    "put_super"
    "sync_fs"
    "freeze_fs"
    "unfreeze_fs"
    "statfs"
    "remount_fs"
    "umount_begin"
    "bdev_try_to_free_page"
REQUIRED
    "dirty_inode"
        TYPED "with_flags" "void (*)(struct inode*, int flags)"
        TYPED "without_flags" "void (*)(struct inode*)"
    "write_inode"
        TYPED "accept_wbc" "int (*)(struct inode*, struct writeback_control*)"
        TYPED "accept_int" "int (*)(struct inode*, int)"
    "drop_inode"
        TYPED "return_int" "int (*)(struct inode*)"
        TYPED "return_void" "void (*)(struct inode*)"
OPTIONAL
    "delete_inode"
    "clear_inode"
    
    "show_devname"
        TYPED "accept_mnt" "int(*)(struct seq_file*, struct vfsmount*)"
        TYPED "accept_dentry" "int(*)(struct seq_file*, struct dentry*)"
    "show_path"
        TYPED "accept_mnt" "int(*)(struct seq_file*, struct vfsmount*)"
        TYPED "accept_dentry" "int(*)(struct seq_file*, struct dentry*)"
REQUIRED
    "show_options"
        TYPED "accept_mnt" "int(*)(struct seq_file*, struct vfsmount*)"
        TYPED "accept_dentry" "int(*)(struct seq_file*, struct dentry*)"
    "show_stats"
        TYPED "accept_mnt" "int(*)(struct seq_file*, struct vfsmount*)"
        TYPED "accept_dentry" "int(*)(struct seq_file*, struct dentry*)"
OPTIONAL
    "write_super"
)

add_interceptor("super_operations_interceptor" "header.yaml" ${operations_list})
