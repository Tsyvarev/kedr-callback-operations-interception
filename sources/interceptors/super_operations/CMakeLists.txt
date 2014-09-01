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

kmodule_configure_operations(evict_inode_operation OPTIONAL "evict_inode")
if(evict_inode_operation)
    # Detect default ->evict_inode implementation.
    check_begin("Detect way of default ->evict_inode implementation")
    if(NOT DEFINED HAS_TRUNCATE_INODE_PAGES_FINAL)
        check_try()
        lookup_module_symbol(res "truncate_inode_pages_final")
        set(HAS_TRUNCATE_INODE_PAGES_FINAL ${res} CACHE INTERNAL
            "Whether truncate_inode_pages_final() function exists."
        )
    endif(NOT DEFINED HAS_TRUNCATE_INODE_PAGES_FINAL)
    if(NOT HAS_TRUNCATE_INODE_PAGES_FINAL AND NOT DEFINED HAS_CLEAR_INODE)
        check_try()
        lookup_module_symbol(res "clear_inode")
        set(HAS_CLEAR_INODE ${res} CACHE INTERNAL
            "Whether clear_inode() function exists."
        )
    endif(NOT HAS_TRUNCATE_INODE_PAGES_FINAL AND NOT DEFINED HAS_CLEAR_INODE)
    
    if(HAS_TRUNCATE_INODE_PAGES_FINAL)
        set(msg_res "use truncate_inode_pages_final")
        configure_file("evict_inode_use_final.yaml"
            "evict_inode.yaml"
            COPY_ONLY
        )
    elseif(HAS_CLEAR_INODE)
        set(msg_res "use clear_inode")
        configure_file("evict_inode_use_clear_inode.yaml"
            "evict_inode.yaml"
            COPY_ONLY
        )
    else(HAS_TRUNCATE_INODE_PAGES_FINAL)
        set(msg_res "use end_writeback")
        configure_file("evict_inode_use_end_writeback.yaml"
            "evict_inode.yaml"
            COPY_ONLY
        )
    endif(HAS_TRUNCATE_INODE_PAGES_FINAL)
    
    check_end(${msg_res})
    
    list(APPEND operations_list ${evict_inode_operation})
endif(evict_inode_operation)

add_interceptor("super_operations_interceptor" "header.yaml" ${operations_list})
