#ifndef FS_INTERCEPTION_H
#define FS_INTERCEPTION_H

/*
 * Bind different objects of file system for intercept their operations.
 * 
 * Binded objects are: file_system_type, super_block, inode, dentry, file.
 * 
 * Creation and destroying of file_system_type objects should be tracked
 * externally using
 * file_system_type_interceptor_{watch, forget, forget_norestore}.
 * 
 * All other file system objects corresponded to watched
 * file_system_type will be tracked automatically.
 * 
 * The only exception is for root inode and other root objects,
 * which are created during file_system_type->mount() call before
 * super block object is watched. For make root objects watched during
 * that call, one should intercept (e.g, with KEDR) normal functions,
 * and watch these objects manually.
 */

#include <kedr-coi/operations_interception.h>

#include <kedr-coi-kernel/interceptors/file_operations_interceptor.h>
#include <kedr-coi-kernel/interceptors/inode_file_operations_interceptor.h>
#include <kedr-coi-kernel/interceptors/file_system_type_interceptor.h>
#include <kedr-coi-kernel/interceptors/super_operations_interceptor.h>
#include <kedr-coi-kernel/interceptors/inode_operations_interceptor.h>
#include <kedr-coi-kernel/interceptors/dentry_operations_interceptor.h>

/*
 * Initialize all interceptors for file system objects and set bindings
 * between them.
 */
int fs_interception_init(void);

/*
 * Destroy bingings between file system objects and destroy interceptors
 * for them.
 */
void fs_interception_destroy(void);

/*
 * Start all interceptors for file system objects.
 */
int fs_interception_start(void);

/*
 * Stop all interceptors for file system objects.
 */
void fs_interception_stop(void);

/********* Useful functions for precise interception scheme ***********/
// Unify interceptors for inode operations and file operations for inode

static inline void inode_operations_all_interceptor_watch(struct inode* inode)
{
    inode_operations_interceptor_watch(inode);
    inode_file_operations_interceptor_watch(inode);
}

static inline void inode_operations_all_interceptor_forget(struct inode* inode)
{
    inode_file_operations_interceptor_forget(inode);
    inode_operations_interceptor_forget(inode);
}

static inline void inode_operations_all_interceptor_forget_norestore(struct inode* inode)
{
    inode_file_operations_interceptor_forget_norestore(inode);
    inode_operations_interceptor_forget_norestore(inode);
}

// Unify interceptors for dentry operations and inode operations for its inode
static inline void dentry_operations_all_interceptor_watch(struct dentry* dentry)
{
    dentry_operations_interceptor_watch(dentry);
    if(dentry->d_inode)
        inode_operations_all_interceptor_watch(dentry->d_inode);
}

static inline void dentry_operations_all_interceptor_forget(struct dentry* dentry)
{
    if(dentry->d_inode)
        inode_operations_all_interceptor_forget(dentry->d_inode);
    dentry_operations_interceptor_forget(dentry);
}

#endif /*FS_INTERCEPTION_H*/