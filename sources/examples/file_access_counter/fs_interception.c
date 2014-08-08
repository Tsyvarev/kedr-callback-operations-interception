/*
 * Implements bindings between interceptors of different file system
 * objects. Interceptors are:
 * 
 * -file_system_type_interceptor
 * -super_operations_interceptor
 * -dentry_operations_interceptor
 * -inode_operations_interceptor
 * -file_operations_interceptor
 * 
 * Bindings are so that all super blocks, dentries, inodes and files
 * belonging to all watched filesystems are watched automatically.
 * 
 * So user should only call file_system_type_interceptor_watch and 
 * file_system_type_interceptor_forget[_norestore] when needed.
 * Watch/forget functions for others interceptors shouldn't be called.
 * 
 * NB: It is better to intercept also root_inode creation functions,
 * and call super_operations_interceptor_watch() for corresponded
 * object.
 */

/* ========================================================================
 * Copyright (C) 2011, Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ======================================================================== */

#include <linux/module.h>

#include <linux/fs.h>
#include <linux/mount.h>

#include "fs_interception.h"

/***************** File system type payload *************************/

/* Determine lifetime of super block(from file_system_type object) */
#if defined(FILE_SYSTEM_TYPE_HAS_GET_SB)
static void fst_get_sb_post_super_lifetime(struct file_system_type* type,
    int flags, const char* name, void* data,
    struct vfsmount* mnt,
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);
    if(returnValue == 0)
    {
        super_operations_interceptor_watch(mnt->mnt_sb);
    }
}
#elif defined(FILE_SYSTEM_TYPE_HAS_MOUNT)
static void fst_mount_post_super_lifetime(struct file_system_type* type,
    int flags, const char* name, void* data,
    struct kedr_coi_operation_call_info* call_info)
{
    struct dentry* kedr_coi_declare_return_value(call_info, returnValue);
    if(!IS_ERR(returnValue))
    {
        super_operations_interceptor_watch(returnValue->d_sb);
    }
}
#else
#error Cannot bind super_block with file_system_type because neither 'get_sb' nor 'mount' is defined for 'file_system_type' structure.
#endif /*FILE_SYSTEM_TYPE_HAS_GET_SB*/

static void fst_kill_sb_post_super_lifetime(struct super_block *sb,
    struct kedr_coi_operation_call_info* call_info)
{
    super_operations_interceptor_forget_norestore(sb);
}

/* Determine lifetime of root dentry and inode(from file_system_type object) */
#if defined(FILE_SYSTEM_TYPE_HAS_GET_SB)
static void fst_get_sb_post_root_lifetime(struct file_system_type* type,
    int flags, const char* name, void* data,
    struct vfsmount* mnt
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);
    if(returnValue == 0)
    {
        dentry_operations_all_interceptor_watch(mnt->mnt_sb->s_root);
    }
}
#elif defined(FILE_SYSTEM_TYPE_HAS_MOUNT)
static void fst_mount_post_root_lifetime(struct file_system_type* type,
    int flags, const char* name, void* data,
    struct kedr_coi_operation_call_info* call_info)
{
    struct dentry* kedr_coi_declare_return_value(call_info, returnValue);
    if(returnValue != NULL)
    {
        dentry_operations_all_interceptor_watch(returnValue);
    }
}
#else
#error Cannot bind root dentry with file_system_type because neither 'get_sb' nor 'mount' is defined for 'file_system_type' structure.
#endif /*FILE_SYSTEM_TYPE_HAS_GET_SB*/

/* Combine all handlers for file system type operations together */
static struct kedr_coi_handler file_system_type_post_handlers[] =
{
#ifdef FILE_SYSTEM_TYPE_HAS_GET_SB
    file_system_type_get_sb_handler(fst_get_sb_post_super_lifetime),
    file_system_type_get_sb_handler(fst_get_sb_post_root_lifetime),
#else
    file_system_type_mount_handler(fst_mount_post_super_lifetime),
    file_system_type_mount_handler(fst_mount_post_root_lifetime),
#endif
    file_system_type_kill_sb_handler(fst_kill_sb_post_super_lifetime),

    kedr_coi_handler_end
};

static struct kedr_coi_payload file_system_type_payload =
{
    .post_handlers = file_system_type_post_handlers
};

/******************** Super block payload *****************************/

/* Determine lifetime of inode object(from super block)*/
static void sops_destory_inode_pre_inode_lifetime(struct inode* inode,
    struct kedr_coi_operation_call_info* call_info)
{
    inode_operations_all_interceptor_forget_norestore(inode);
    //pr_info("Inode at %p was destroyed.", inode);
}

/* Combine all handler for super_operations together */
static struct kedr_coi_handler super_operations_pre_handlers[] =
{
    super_operations_destroy_inode_handler_external(sops_destory_inode_pre_inode_lifetime),
    kedr_coi_handler_end
};

static struct kedr_coi_payload super_operations_payload =
{
    .mod = THIS_MODULE,
    
    .pre_handlers = super_operations_pre_handlers,
};


/******************** Dentry payload **********************************/

/* Determine lifetime of dentry object(from object itself)*/

static void dops_d_release_pre_dentry_lifetime(struct dentry* dentry,
    struct kedr_coi_operation_call_info* call_info)
{
    // inode lifetime is determine in another way
    dentry_operations_interceptor_forget(dentry);
}

/* Update watching for dentry object */
static void dops_d_revalidate_post_dentry_watch(struct dentry* dentry,
#ifdef DENTRY_OPERATIONS_D_REVALIDATE_ACCEPT_UINT
    unsigned int excl,
#else
    struct nameidata* nd,
#endif
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);
    if(returnValue > 0)
    {
        dentry_operations_all_interceptor_watch(dentry);
    }
}

/* Combine all handlers for dentry operations together*/
static struct kedr_coi_handler dentry_operations_pre_handlers[] =
{
    dentry_operations_d_release_handler_external(dops_d_release_pre_dentry_lifetime),
    kedr_coi_handler_end
};

static struct kedr_coi_handler dentry_operations_post_handlers[] =
{
    dentry_operations_d_revalidate_handler(dops_d_revalidate_post_dentry_watch),
    kedr_coi_handler_end
};

static struct kedr_coi_payload dentry_operations_payload =
{
    .mod = THIS_MODULE,
    
    .pre_handlers = dentry_operations_pre_handlers,
    .post_handlers = dentry_operations_post_handlers
};

/********************* Inode payload **********************************/

/* Determine lifetime of dentry object from another inode object*/
static void iops_lookup_post_dentry_lifetime(struct inode *inode,
	struct dentry *dentry,
#if defined(INODE_OPERATIONS_LOOKUP_ACCEPT_BOOL)
    bool excl,
#elif defined(INODE_OPERATIONS_LOOKUP_ACCEPT_UINT)
    unsigned int excl,
#else
    struct nameidata *nd,
#endif /* INODE_OPERATIONS_LOOKUP_ACCEPT_BOOL */
    struct kedr_coi_operation_call_info* call_info)
{
    struct dentry* kedr_coi_declare_return_value(call_info, returnValue);

    if(IS_ERR(returnValue)) return;
    
    else if(returnValue)
    {
        dentry_operations_all_interceptor_watch(returnValue);
    }
    else
    {
        dentry_operations_all_interceptor_watch(dentry);
    }
}

static void iops_mkdir_post_dentry_lifetime(struct inode* dir,
    struct dentry* dentry,
#ifdef INODE_OPERATIONS_MKDIR_ACCEPT_UMODE
    umode_t mode,
#else
    int mode,
#endif
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);

    if(returnValue == 0)
        dentry_operations_all_interceptor_watch(dentry);
}

static void iops_link_post_dentry_lifetime(struct dentry* old_dentry,
    struct inode* dir, struct dentry* dentry,
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);

    if(returnValue == 0)
        dentry_operations_all_interceptor_watch(dentry);
}



static void iops_create_post_dentry_lifetime(struct inode* dir,
    struct dentry* dentry,
#ifdef INODE_OPERATIONS_CREATE_ACCEPT_UMODE
    umode_t mode,
#else
    int mode,
#endif /* INODE_OPERATIONS_CREATE_ACCEPT_UMODE */
#ifdef INODE_OPERATIONS_CREATE_ACCEPT_BOOL
    bool excl,
#else
    struct nameidata* nm,
#endif /* INODE_OPERATIONS_CREATE_ACCCEPT_BOOL */
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);
    if(returnValue == 0)
        dentry_operations_all_interceptor_watch(dentry);
}

/* Update watching for inode object(from object itself)*/
static void iops_lookup_post_inode_watch(struct inode *inode,
	struct dentry *dentry,
#if defined(INODE_OPERATIONS_LOOKUP_ACCEPT_BOOL)
    bool excl,
#elif defined(INODE_OPERATIONS_LOOKUP_ACCEPT_UINT)
    unsigned int excl,
#else
    struct nameidata *nd,
#endif /* INODE_OPERATIONS_LOOKUP_ACCEPT_BOOL */
    struct kedr_coi_operation_call_info* call_info)
{
    inode_operations_all_interceptor_watch(inode);
}


/* Update watching for dentry object(from its inode object)*/
static void iops_getattr_post_dentry_watch(struct vfsmount* mnt,
    struct dentry* dentry, struct kstat* stat,
    struct kedr_coi_operation_call_info* call_info)
{
    dentry_operations_all_interceptor_watch(dentry);
}


// Combine all handlers for inode operations
static struct kedr_coi_handler inode_operations_post_handlers[] =
{
    inode_operations_lookup_handler(iops_lookup_post_dentry_lifetime),
    inode_operations_mkdir_handler(iops_mkdir_post_dentry_lifetime),
    inode_operations_create_handler(iops_create_post_dentry_lifetime),
    inode_operations_link_handler(iops_link_post_dentry_lifetime),
    
    inode_operations_lookup_handler(iops_lookup_post_inode_watch),
    inode_operations_getattr_handler(iops_getattr_post_dentry_watch),
    kedr_coi_handler_end,
};

static struct kedr_coi_payload inode_operations_payload =
{
    .mod = THIS_MODULE,
    
    .post_handlers = inode_operations_post_handlers,
};


/***********************File payload***********************************/

/* Update watching for the file object(from object itself)*/
static void fops_open_post_file_watch(struct inode* inode,
    struct file* filp,
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);
    if(returnValue == 0)
    {
        file_operations_interceptor_watch(filp);
    }
}


/* Determine lifetime of file object(from object iteself) */
static void fops_open_post_file_lifetime(struct inode* inode,
    struct file* filp,
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);
    if(returnValue)
    {
        //error-path
        file_operations_interceptor_forget(filp);
    }
}

static void fops_release_post_file_lifetime(struct inode* inode,
    struct file* filp,
    struct kedr_coi_operation_call_info* call_info)
{
    file_operations_interceptor_forget(filp);
}

/* 
 * Update interception information about inode(from file).
 * 
 * Because this handler is specific for file, created via watched(!)
 * inode, it added to payload for inode-file factory interceptor.
 * 
 * So, this handler will not affect on file, created for character device.
 */
static void fops_release_post_inode_update(struct inode* inode,
    struct file* filp,
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);

    /*
     * We only interested in files, created from watched inodes.
     * 
     * Because only those inodes may be watched, which are corresponds
     * to filesystem, we only interested in regular files, dirs and links.
     */
    if(inode->i_mode & (S_IFLNK | S_IFREG | S_IFDIR)) return;

    if(returnValue == 0)
    {
       inode_operations_all_interceptor_watch(inode);
    }
}


// Combine all handlers for file together
static struct kedr_coi_handler file_operations_post_handlers[] =
{
    file_operations_open_handler(fops_open_post_file_watch),
    file_operations_open_handler(fops_open_post_file_lifetime),
    file_operations_release_handler_external(fops_release_post_file_lifetime),
    file_operations_release_handler(fops_release_post_inode_update),
    kedr_coi_handler_end
};

static struct kedr_coi_payload file_operations_payload =
{
    .mod = THIS_MODULE,
    
    .post_handlers = file_operations_post_handlers,
};

int fs_interception_start(void)
{
    int result;
    
    result = file_operations_interceptor_start();
    if(result) goto err_file_operations;
    
    result = inode_operations_interceptor_start();
    if(result) goto err_inode_operations;
    
    result = dentry_operations_interceptor_start();
    if(result) goto err_dentry_operations;
    
    result = super_operations_interceptor_start();
    if(result) goto err_super_operations;
    
    result = file_system_type_interceptor_start();
    if(result) goto err_file_system_type;
    
    return 0;

err_file_system_type:
    super_operations_interceptor_stop();
err_super_operations:
    dentry_operations_interceptor_stop();
err_dentry_operations:
    inode_operations_interceptor_stop();
err_inode_operations:
    file_operations_interceptor_stop();
err_file_operations:
    
    return result;
}


void fs_interception_stop(void)
{
    file_system_type_interceptor_stop();
    super_operations_interceptor_stop();
    dentry_operations_interceptor_stop();
    inode_operations_interceptor_stop();
    file_operations_interceptor_stop();
}

static void trace_unforgotten_file(const struct file* filp)
{
    pr_info("File object %p wasn't forgotten by interceptor.", filp);
}

static void trace_unforgotten_inode(const struct inode* inode)
{
    pr_info("Inode object %p wasn't forgotten by interceptor.", inode);
}

static void trace_unforgotten_inode_for_file(const struct inode* inode)
{
    pr_info("Inode object %p wasn't forgotten by foreign interceptor for file operations.", inode);
}

static void trace_unforgotten_dentry(const struct dentry* dentry)
{
    pr_info("Dentry object %p wasn't forgotten by interceptor.", dentry);
}

static void trace_unforgotten_super(const struct super_block* super)
{
    pr_info("Super block %p wasn't forgotten by interceptor.", super);
}

static void trace_unforgotten_fst(const struct file_system_type* type)
{
    pr_info("File system type %p wasn't forgotten by interceptor.", type);
}


int fs_interception_init(void)
{
    int result;
    
    // Create interceptors
    result = file_operations_interceptor_init();
    if(result) goto err_file_operations;
    
    file_operations_interceptor_trace_unforgotten_object(
        &trace_unforgotten_file);
    
    result = file_system_type_interceptor_init();
    if(result) goto err_file_system_type;
    
    file_system_type_interceptor_trace_unforgotten_object(
        &trace_unforgotten_fst);
    
    result = super_operations_interceptor_init();
    if(result) goto err_super_operations;
    
    super_operations_interceptor_trace_unforgotten_object(
        &trace_unforgotten_super);
    
    result = dentry_operations_interceptor_init();
    if(result) goto err_dentry_operations;

    dentry_operations_interceptor_trace_unforgotten_object(
        &trace_unforgotten_dentry);

    result = inode_operations_interceptor_init();
    if(result) goto err_inode_operations;
    
    inode_operations_interceptor_trace_unforgotten_object(
        &trace_unforgotten_inode);
    
    result = inode_file_operations_interceptor_init(
        file_operations_interceptor_factory_interceptor_create);
    if(result) goto err_inode_file_operations;
    
    inode_file_operations_interceptor_trace_unforgotten_object(
        &trace_unforgotten_inode_for_file);

    // Set connections between interceptors using payloads
    result = file_operations_interceptor_payload_register(
        &file_operations_payload);
    if(result) goto err_file_operations_payload;

    result = inode_operations_interceptor_payload_register(
        &inode_operations_payload);
    if(result) goto err_inode_operations_payload;
    
    result = dentry_operations_interceptor_payload_register(
        &dentry_operations_payload);
    if(result) goto err_dentry_operations_payload;

    result = super_operations_interceptor_payload_register(
        &super_operations_payload);
    if(result) goto err_super_operations_payload;

    result = file_system_type_interceptor_payload_register(
        &file_system_type_payload);
    if(result) goto err_file_system_type_payload;

    return 0;

err_file_system_type_payload:
    super_operations_interceptor_payload_unregister(&super_operations_payload);
err_super_operations_payload:
    dentry_operations_interceptor_payload_unregister(&dentry_operations_payload);
err_dentry_operations_payload:
    inode_operations_interceptor_payload_unregister(&inode_operations_payload);
err_inode_operations_payload:
    file_operations_interceptor_payload_unregister(&file_operations_payload);
err_file_operations_payload:
//
    inode_file_operations_interceptor_destroy();
err_inode_file_operations:
    inode_operations_interceptor_destroy();
err_inode_operations:
    dentry_operations_interceptor_destroy();
err_dentry_operations:
    super_operations_interceptor_destroy();
err_super_operations:
    file_system_type_interceptor_destroy();
err_file_system_type:
    file_operations_interceptor_destroy();
err_file_operations:
    
    return result;
}

void fs_interception_destroy(void)
{
    file_system_type_interceptor_payload_unregister(&file_system_type_payload);
    super_operations_interceptor_payload_unregister(&super_operations_payload);
    dentry_operations_interceptor_payload_unregister(&dentry_operations_payload);
    inode_operations_interceptor_payload_unregister(&inode_operations_payload);
    file_operations_interceptor_payload_unregister(&file_operations_payload);
    //
    inode_file_operations_interceptor_destroy();
    inode_operations_interceptor_destroy();
    dentry_operations_interceptor_destroy();
    super_operations_interceptor_destroy();
    file_system_type_interceptor_destroy();
    file_operations_interceptor_destroy();
}
