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

#endif /*FS_INTERCEPTION_H*/