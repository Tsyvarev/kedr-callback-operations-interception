/*
 * Add counter of readings to the module implemented character device.
 * 
 * The only thing is needed from module - annotation of kernel
 * functions which add and remove character device.
 * (And adding some initialization/deinitialization routins).
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

#include <kedr-coi/interceptors/file_operations_interceptor.h>
#include "cdev_file_operations_interceptor.h"

unsigned read_counter = 0;
module_param(read_counter, uint, S_IRUGO);

/** Intercept reads from file and determine lifetime of file object */

/* Counter update */
static void file_operation_read_post_counter_update(struct file* filp,
    char __user* buf, size_t count, loff_t* f_pos, ssize_t returnValue,
    struct kedr_coi_operation_call_info* call_info)
{
    if(returnValue > 0) read_counter++;
}

/* Set watching for the file (file operations interceptor)*/
static void file_operation_open_post_file_lifetime(struct inode* inode,
    struct file* filp, int returnValue,
    struct kedr_coi_operation_call_info* call_info)
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

static void file_operation_release_post_file_lifetime(struct inode* inode,
    struct file* filp, int returnValue,
    struct kedr_coi_operation_call_info* call_info)
{
    if(returnValue == 0)
    {
        file_operations_interceptor_forget(filp);
    }
}

static struct kedr_coi_post_handler file_operations_post_handlers[] =
{
    file_operations_read_post(file_operation_read_post_counter_update),
    file_operations_open_post(file_operation_open_post_file_lifetime),
    file_operations_release_post_external(file_operation_release_post_file_lifetime),
    kedr_coi_post_handler_end
};

struct kedr_coi_payload file_operations_payload =
{
    /*
     *  Because this payload is embedded into module implemented
     * character device, 'mod' field shouldn't be set for payload.
     * 
     * Otherwise one will unable to unload module.
     */
    
    .post_handlers = file_operations_post_handlers,
};

extern int functions_support_register(void);
extern void functions_support_unregister(void);

int read_counter_init(void)
{
    int result;
    
    result = file_operations_interceptor_init(NULL);
    if(result) goto err_file_operations;
    
    result = cdev_file_operations_interceptor_init(
        file_operations_interceptor_foreign_interceptor_create,
        NULL);

    if(result) goto err_cdev_file_operations;

    result = file_operations_interceptor_payload_register(&file_operations_payload);
    if(result) goto err_file_operations_payload;

    result = file_operations_interceptor_start();
    if(result) goto err_file_operations_start;
    
    read_counter = 0;
    return 0;

err_file_operations_start:
    file_operations_interceptor_payload_unregister(&file_operations_payload);
err_file_operations_payload:
    cdev_file_operations_interceptor_destroy();
err_cdev_file_operations:
    file_operations_interceptor_destroy();
err_file_operations:
    
    return result;
}

void read_counter_destroy(void)
{
    file_operations_interceptor_stop();
    file_operations_interceptor_payload_unregister(&file_operations_payload);
    cdev_file_operations_interceptor_destroy();
    file_operations_interceptor_destroy();
}
