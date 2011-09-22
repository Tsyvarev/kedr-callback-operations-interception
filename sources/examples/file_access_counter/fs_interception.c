/*
 * Implements binding of file system objects.
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

//*Unify interceptors for inode operations and file operations for inode*

static void inode_operations_all_interceptor_watch(struct inode* inode)
{
    inode_operations_interceptor_watch(inode);
    inode_file_operations_interceptor_watch(inode);
}

// Not used
//static void inode_operations_all_interceptor_forget(struct inode* inode)
//{
    //inode_file_operations_interceptor_forget(inode);
    //inode_operations_interceptor_forget(inode);
//}


static void inode_operations_all_interceptor_forget_norestore(struct inode* inode)
{
    inode_file_operations_interceptor_forget_norestore(inode);
    inode_operations_interceptor_forget_norestore(inode);
}

/* Unify interceptors for dentry operations and inode operations for its inode */
static void dentry_operations_all_interceptor_watch(struct dentry* dentry)
{
    dentry_operations_interceptor_watch(dentry);
    if(dentry->d_inode)
        inode_operations_all_interceptor_watch(dentry->d_inode);
}

// Not used
//static void dentry_operations_all_interceptor_forget(struct dentry* dentry)
//{
    //if(dentry->d_inode)
        //inode_operations_all_interceptor_forget(dentry->d_inode);
    //dentry_operations_interceptor_forget(dentry);
//}


/***************** File system type payload *************************/

/* Determine lifetime of super block(from file_system_type object) */
#if defined(FILE_SYSTEM_TYPE_HAS_GET_SB)
static void fst_get_sb_post_super_lifetime(struct file_system_type* type,
    int flags, const char* name, void* data,
    struct vfsmount* mnt, int returnValue)
{
    if(returnValue == 0)
    {
        super_operations_interceptor_watch(mnt->mnt_sb);
    }
}
#elif defined(FILE_SYSTEM_TYPE_HAS_MOUNT)
static void fst_mount_post_super_lifetime(struct file_system_type* type,
    int flags, const char* name, void* data,
    struct dentry* returnValue)
{
    if(returnValue != NULL)
    {
        super_operations_interceptor_watch(returnValue->d_sb);
    }
}
#else
#error Cannot bind super_block with file_system_type because neither 'get_sb' nor 'mount' is defined for 'file_system_type' structure.
#endif /*FILE_SYSTEM_TYPE_HAS_GET_SB*/

static void fst_kill_sb_post_super_lifetime(struct super_block *sb)
{
    super_operations_interceptor_forget_norestore(sb);
}

/* Determine lifetime of root dentry and inode(from file_system_type object) */
#if defined(FILE_SYSTEM_TYPE_HAS_GET_SB)
static void fst_get_sb_post_root_lifetime(struct file_system_type* type,
    int flags, const char* name, void* data,
    struct vfsmount* mnt, int returnValue)
{
    if(returnValue == 0)
    {
        dentry_operations_all_interceptor_watch(mnt->mnt_sb->s_root);
    }
}
#elif defined(FILE_SYSTEM_TYPE_HAS_MOUNT)
static void fst_mount_post_root_lifetime(struct file_system_type* type,
    int flags, const char* name, void* data,
    struct dentry* returnValue)
{
    if(returnValue != NULL)
    {
        dentry_operations_all_interceptor_watch(returnValue);
    }
}
#else
#error Cannot bind root dentry with file_system_type because neither 'get_sb' nor 'mount' is defined for 'file_system_type' structure.
#endif /*FILE_SYSTEM_TYPE_HAS_GET_SB*/

/* Combine all handlers for file system type operations together */
static struct kedr_coi_post_handler file_system_type_post_handlers[] =
{
#ifdef FILE_SYSTEM_TYPE_HAS_GET_SB
    file_system_type_get_sb_post(fst_get_sb_post_super_lifetime),
    file_system_type_get_sb_post(fst_get_sb_post_root_lifetime),
#else
    file_system_type_mount_post(fst_mount_post_super_lifetime),
    file_system_type_mount_post(fst_mount_post_root_lifetime),
#endif
    file_system_type_kill_sb_post(fst_kill_sb_post_super_lifetime),

    kedr_coi_post_handler_end
};

static struct kedr_coi_payload file_system_type_payload =
{
    .post_handlers = file_system_type_post_handlers
};

/******************** Super block payload *****************************/

/* Determine lifetime of inode object(from super block)*/
static void sops_destory_inode_pre_inode_lifetime(struct inode* inode)
{
    inode_operations_all_interceptor_forget_norestore(inode);
}

/* Combine all handler for super_operations together */
static struct kedr_coi_pre_handler super_operations_pre_handlers[] =
{
    super_operations_destroy_inode_pre(sops_destory_inode_pre_inode_lifetime),
    kedr_coi_pre_handler_end
};

static struct kedr_coi_payload super_operations_payload =
{
    .mod = THIS_MODULE,
    
    .pre_handlers = super_operations_pre_handlers,
};


/******************** Dentry payload **********************************/

/* Determine lifetime of dentry object(from object itself)*/

static void dops_d_release_pre_dentry_lifetime(struct dentry* dentry)
{
    // inode lifetime is determine in another way
    dentry_operations_interceptor_forget(dentry);
}

/* Update watching for dentry object */
static void dops_d_revalidate_post_dentry_watch(struct dentry* dentry,
    struct nameidata* nd, int returnValue)
{
    if(returnValue > 0)
    {
        dentry_operations_all_interceptor_watch(dentry);
    }
}

/* Combine all handlers for dentry operations together*/
static struct kedr_coi_pre_handler dentry_operations_pre_handlers[] =
{
    dentry_operations_d_release_pre(dops_d_release_pre_dentry_lifetime),
    kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler dentry_operations_post_handlers[] =
{
    dentry_operations_d_revalidate_post(dops_d_revalidate_post_dentry_watch),
    kedr_coi_post_handler_end
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
	struct dentry *dentry, struct nameidata *nd,
    struct dentry *returnValue)
{
    if(returnValue && !IS_ERR(returnValue))
    {
        dentry_operations_all_interceptor_watch(returnValue);
    }
}

/* Update watching for inode object(from object itself)*/
static void iops_lookup_post_inode_watch(struct inode *inode,
	struct dentry *dentry, struct nameidata *nd,
    struct dentry *returnValue)
{
    inode_operations_all_interceptor_watch(inode);
}


/* Update watching for dentry object(from its inode object)*/
static void iops_getattr_post_dentry_watch(struct vfsmount* mnt,
    struct dentry* dentry, struct kstat* stat, int returnValue)
{
    dentry_operations_all_interceptor_watch(dentry);
}

//.. others inode operations

// Combine all handlers for inode operations
static struct kedr_coi_post_handler inode_operations_post_handlers[] =
{
    inode_operations_lookup_post(iops_lookup_post_dentry_lifetime),
    inode_operations_lookup_post(iops_lookup_post_inode_watch),
    inode_operations_getattr_post(iops_getattr_post_dentry_watch),
    kedr_coi_post_handler_end,
};

static struct kedr_coi_payload inode_operations_payload =
{
    .mod = THIS_MODULE,
    
    .post_handlers = inode_operations_post_handlers,
};


/***********************File payload***********************************/

/* Update watching for the file object(from object itself)*/
static void fops_open_post_file_watch(struct inode* inode,
    struct file* filp, int returnValue)
{
    if(returnValue == 0)
    {
        file_operations_interceptor_watch(filp);
    }
}


/* Determine lifetime of file object(from object iteself) */
static void fops_open_post_file_lifetime(struct inode* inode,
    struct file* filp, int returnValue)
{
    if(returnValue)
    {
        //error-path
        file_operations_interceptor_forget(filp);
    }
}

static void fops_release_post_file_lifetime(struct inode* inode,
    struct file* filp, int returnValue)
{
    if(returnValue == 0)
    {
        file_operations_interceptor_forget(filp);
    }
}

// Combine all handlers for file together
static struct kedr_coi_post_handler file_operations_post_handlers[] =
{
    file_operations_open_post(fops_open_post_file_watch),
    file_operations_open_post(fops_open_post_file_lifetime),
    file_operations_release_post(fops_release_post_file_lifetime),
    kedr_coi_post_handler_end
};

static struct kedr_coi_payload file_operations_payload =
{
    .mod = THIS_MODULE,
    
    .post_handlers = file_operations_post_handlers,
};




int fs_interception_start(void)
{
    // TODO: errors should be processed.
    file_operations_interceptor_start();
    inode_operations_interceptor_start();
    dentry_operations_interceptor_start();
    super_operations_interceptor_start();
    file_system_type_interceptor_start();
    
    return 0;
}


void fs_interception_stop(void)
{
    file_system_type_interceptor_stop();
    super_operations_interceptor_stop();
    dentry_operations_interceptor_stop();
    inode_operations_interceptor_stop();
    file_operations_interceptor_stop();
}

static void trace_unforgotten_file(struct file* filp)
{
    pr_info("File object %p wasn't forgotten by interceptor.", filp);
}

static void trace_unforgotten_inode(struct inode* inode)
{
    pr_info("Inode object %p wasn't forgotten by interceptor.", inode);
}

static void trace_unforgotten_inode_for_file(struct inode* inode)
{
    pr_info("Inode object %p wasn't forgotten by foreign interceptor for file operations.", inode);
}

static void trace_unforgotten_dentry(struct dentry* dentry)
{
    pr_info("Dentry object %p wasn't forgotten by interceptor.", dentry);
}

static void trace_unforgotten_super(struct super_block* super)
{
    pr_info("Super block %p wasn't forgotten by interceptor.", super);
}

static void trace_unforgotten_fst(struct file_system_type* type)
{
    pr_info("File system type %p wasn't forgotten by interceptor.", type);
}


int fs_interception_init(void)
{
    int result;
    
    result = file_operations_interceptor_init(&trace_unforgotten_file);
    if(result) goto err_file_operations;
    
    result = file_system_type_interceptor_init(&trace_unforgotten_fst);
    if(result) goto err_file_system_type;
    
    result = super_operations_interceptor_init(&trace_unforgotten_super);
    if(result) goto err_super_operations;
    
    result = dentry_operations_interceptor_init(&trace_unforgotten_dentry);
    if(result) goto err_dentry_operations;

    result = inode_operations_interceptor_init(&trace_unforgotten_inode);
    if(result) goto err_inode_operations;
    
    result = inode_file_operations_interceptor_init(
        file_operations_interceptor_foreign_interceptor_create,
        &trace_unforgotten_inode_for_file);
    if(result) goto err_inode_file_operations;

    //
    result = file_operations_interceptor_payload_register(&file_operations_payload);
    if(result) goto err_file_operations_payload;

    result = inode_operations_interceptor_payload_register(&inode_operations_payload);
    if(result) goto err_inode_operations_payload;
    
    result = dentry_operations_interceptor_payload_register(&dentry_operations_payload);
    if(result) goto err_dentry_operations_payload;

    result = super_operations_interceptor_payload_register(&super_operations_payload);
    if(result) goto err_super_operations_payload;

    result = file_system_type_interceptor_payload_register(&file_system_type_payload);
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
