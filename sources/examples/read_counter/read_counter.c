/*
 * Count reads from file, created for character device.
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
#include <linux/cdev.h>

#include <kedr/core/kedr.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");

#include <kedr-coi/interceptors/file_operations_interceptor.h>
#include "cdev_file_operations_interceptor.h"

unsigned read_counter = 0;
module_param(read_counter, uint, S_IRUGO);

/* Counter update */
static void file_operation_read_post(struct file* filp, char __user* buf,
    size_t count, loff_t* f_pos, ssize_t returnValue)
{
    pr_info("Post handler for read() file operation was called. Position is %d.",
        (int)*f_pos);
    if(returnValue > 0) read_counter++;
}

/* Set watching for the file (file operations interceptor)*/
static void file_operation_open_post(struct inode* inode,
    struct file* filp, int returnValue)
{
    if(returnValue == 0)
    {
        //update
        file_operations_interceptor_watch(filp);
    }
    else
    {
        //error-path
        file_operations_interceptor_forget(filp);
    }
}

static void file_operation_release_post(struct inode* inode,
    struct file* filp, int returnValue)
{
    if(returnValue == 0)
    {
        file_operations_interceptor_forget(filp);
    }
}

static struct kedr_coi_post_handler file_operations_post_handlers[] =
{
    {
        .operation_offset = offsetof(struct file_operations, read),
        .func = file_operation_read_post
    },
    {
        .operation_offset = offsetof(struct file_operations, open),
        .func = file_operation_open_post
    },
    {
        .operation_offset = offsetof(struct file_operations, release),
        .func = file_operation_release_post
    },
    {
        .operation_offset = -1
    }
};

struct kedr_coi_payload file_operations_payload =
{
    .mod = THIS_MODULE,
    
    .post_handlers = file_operations_post_handlers,
};


/* Set watching for the file(cdev foreign operations interceptor) */
static void cdev_file_operations_on_create_handler(struct file* filp)
{
    file_operations_interceptor_watch(filp);
}

kedr_coi_foreign_handler_t cdev_file_operations_on_create_handlers[] =
{
    (kedr_coi_foreign_handler_t)cdev_file_operations_on_create_handler,
    NULL
};


struct kedr_coi_foreign_payload cdev_file_operations_payload =
{
    .mod = THIS_MODULE,
    
    .on_create_handlers = cdev_file_operations_on_create_handlers,
};

/* Set wathcing for the file operations in cdev(KEDR payloads) */
static void cdev_add_pre(struct cdev* dev, dev_t devno, unsigned count)
{
    cdev_file_operations_interceptor_watch(dev);
}


static void cdev_add_post(struct cdev* dev, dev_t devno, unsigned count, int returnValue)
{
    //error-path
    if(returnValue)
        cdev_file_operations_interceptor_forget(dev);
}

static void cdev_del_post(struct cdev* dev)
{
    cdev_file_operations_interceptor_forget(dev);
}

struct kedr_pre_pair pre_pairs[] = 
{
    {
        .orig = cdev_add,
        .pre = cdev_add_pre
    },
    {
        .orig = NULL
    }
};

struct kedr_post_pair post_pairs[] = 
{
    {
        .orig = cdev_add,
        .post = cdev_add_post
    },
    {
        .orig = cdev_del,
        .post = cdev_del_post
    },
    {
        .orig = NULL
    }
};


void on_target_load(struct module* m)
{
    file_operations_interceptor_start();
    cdev_file_operations_interceptor_start();
    
    read_counter = 0;
}

void on_target_unload(struct module* m)
{
    file_operations_interceptor_stop(NULL);
    cdev_file_operations_interceptor_stop(NULL);
}


struct kedr_payload payload =
{
    .mod = THIS_MODULE,
    
    .pre_pairs = pre_pairs,
    .post_pairs = post_pairs,
    
    .target_load_callback = on_target_load,
    .target_unload_callback = on_target_unload
};

extern int functions_support_register(void);
extern void functions_support_unregister(void);

static int __init read_counter_module_init(void)
{
    int result;
    
    result = file_operations_interceptor_init();
    if(result) goto err_file_operations;
    
    result = cdev_file_operations_interceptor_init();
    if(result) goto err_cdev_file_operations;

    result = file_operations_interceptor_payload_register(&file_operations_payload);
    if(result) goto err_file_operations_payload;

    result = cdev_file_operations_interceptor_payload_register(&cdev_file_operations_payload);
    if(result) goto err_cdev_file_operations_payload;

    result = functions_support_register();
    if(result) goto err_functions_support;

    result = kedr_payload_register(&payload);
    if(result) goto err_payload;

    return 0;

err_payload:
    functions_support_unregister();
err_functions_support:
    cdev_file_operations_interceptor_payload_unregister(&cdev_file_operations_payload);
err_cdev_file_operations_payload:
    file_operations_interceptor_payload_unregister(&file_operations_payload);
err_file_operations_payload:
    cdev_file_operations_interceptor_destroy();
err_cdev_file_operations:
    file_operations_interceptor_destroy();
err_file_operations:
    
    return result;
}

static void __exit read_counter_module_exit(void)
{
    kedr_payload_unregister(&payload);
    functions_support_unregister();
    cdev_file_operations_interceptor_payload_unregister(&cdev_file_operations_payload);
    file_operations_interceptor_payload_unregister(&file_operations_payload);
    cdev_file_operations_interceptor_destroy();
    file_operations_interceptor_destroy();
}

module_init(read_counter_module_init);
module_exit(read_counter_module_exit);