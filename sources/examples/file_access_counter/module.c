/*
 * Count opening files under detected file system.
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
#include <linux/moduleparam.h>

#include <linux/fs.h>

#include <kedr/core/kedr.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");

#include "fs_interception.h"

// Counter of regular files openings
unsigned file_counter = 0;
module_param(file_counter, uint, S_IRUGO);

// Counter of file system mounts
static unsigned mount_counter = 0;
module_param(mount_counter, uint, S_IRUGO);



/* Update file counter */
static void fops_open_post_update_file_counter(struct inode* inode,
    struct file* filp,
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);
    if((returnValue == 0) && S_ISREG(inode->i_mode))
    {
        file_counter ++;
    }
}

// Combine all handlers for file together
static struct kedr_coi_handler file_operations_post_handlers[] =
{
    file_operations_open_handler_external(fops_open_post_update_file_counter),
    kedr_coi_handler_end
};

struct kedr_coi_payload file_operations_payload =
{
    .mod = THIS_MODULE,
    
    .post_handlers = file_operations_post_handlers,
};


/* Update mount counter */
#if defined(FILE_SYSTEM_TYPE_HAS_GET_SB)
static void fst_get_sb_post_mount_counter(struct file_system_type* type,
    int flags, const char* name, void* data,
    struct vfsmount* mnt,
    struct kedr_coi_operation_call_info* call_info)
{
    int kedr_coi_declare_return_value(call_info, returnValue);
    if(returnValue == 0)
    {
        mount_counter++;
    }
}
#else
static void fst_mount_post_mount_counter(struct file_system_type* type,
    int flags, const char* name, void* data,
    struct kedr_coi_operation_call_info* call_info)
{
    struct dentry* kedr_coi_declare_return_value(call_info, returnValue);
    if(returnValue != NULL)
    {
        mount_counter++;
    }
}
#endif /*FILE_SYSTEM_TYPE_HAS_GET_SB*/

/* Combine all handlers for file system type operations together */
static struct kedr_coi_handler file_system_type_post_handlers[] =
{
#if defined(FILE_SYSTEM_TYPE_HAS_GET_SB)
    file_system_type_get_sb_handler(fst_get_sb_post_mount_counter),
#else
    file_system_type_mount_handler(fst_mount_post_mount_counter),
#endif
    kedr_coi_handler_end
};

static struct kedr_coi_payload file_system_type_payload =
{
    .post_handlers = file_system_type_post_handlers,
};

/* Define lifetime of file system type(from global functions) */
static void register_filesystem_pre_fst_lifetime(
    struct file_system_type* fs)
{
    file_system_type_interceptor_watch(fs);
}

static void register_filesystem_post_fst_lifetime(
    struct file_system_type* fs, int returnValue)
{
    if(returnValue) //error path
        file_system_type_interceptor_forget(fs);
}

static void unregister_filesystem_post_fst_lifetime(
    struct file_system_type* fs, int returnValue)
{
    if(!returnValue)
        file_system_type_interceptor_forget(fs);
}

static struct kedr_pre_pair pre_pairs[] = 
{
    {
        .orig = register_filesystem,
        .pre = register_filesystem_pre_fst_lifetime
    },
    {
        .orig = NULL
    }
};

static struct kedr_post_pair post_pairs[] = 
{
    {
        .orig = register_filesystem,
        .post = register_filesystem_post_fst_lifetime
    },
    {
        .orig = unregister_filesystem,
        .post = unregister_filesystem_post_fst_lifetime
    },
    {
        .orig = NULL
    }
};


static void on_target_load(struct module* m)
{
    fs_interception_start();
    
    file_counter = 0;
}

static void on_target_unload(struct module* m)
{
    fs_interception_stop();
}


struct kedr_payload payload =
{
    .mod = THIS_MODULE,
    
    .pre_pairs = pre_pairs,
    .post_pairs = post_pairs,
    
    .target_load_callback = on_target_load,
    .target_unload_callback = on_target_unload
};

/*
 *  These two functions are defined in 'functions_support.c',
 * generated from functions_support.data.
 */
extern int functions_support_register(void);
extern void functions_support_unregister(void);

static int __init file_access_counter_module_init(void)
{
    int result;
    
    result = fs_interception_init();
    if(result) goto err_fs_interception;
//
    
    result = file_operations_interceptor_payload_register(&file_operations_payload);
    if(result) goto err_file_operations_payload;

    result = file_system_type_interceptor_payload_register(&file_system_type_payload);
    if(result) goto err_file_system_type_payload;

    result = functions_support_register();
    if(result) goto err_functions_support;

    result = kedr_payload_register(&payload);
    if(result) goto err_payload;

    return 0;

err_payload:
    functions_support_unregister();
err_functions_support:
    file_system_type_interceptor_payload_unregister(&file_system_type_payload);
err_file_system_type_payload:
    file_operations_interceptor_payload_unregister(&file_operations_payload);
err_file_operations_payload:
//
    fs_interception_destroy();
err_fs_interception:

    return result;
}

static void __exit file_access_counter_module_exit(void)
{
    kedr_payload_unregister(&payload);
    functions_support_unregister();
    file_system_type_interceptor_payload_unregister(&file_system_type_payload);
    file_operations_interceptor_payload_unregister(&file_operations_payload);
    //
    fs_interception_destroy();
}

module_init(file_access_counter_module_init);
module_exit(file_access_counter_module_exit);