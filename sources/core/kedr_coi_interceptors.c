/*
 * Implementation of interceptors of different types.
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

#include <kedr-coi/operations_interception.h>

#include "kedr_coi_instrumentor_internal.h"
#include "payloads.h"

#include <linux/slab.h>
#include <linux/module.h> /* for kedr_coi_is_module_address() */

// Return pointer to the operations struct in the object
static const void* indirect_operations(const void* object,
    size_t operations_field_offset)
{
    return *(const void**)((const char*) object + operations_field_offset);
}

static const void** indirect_operations_p(void* object,
    size_t operations_field_offset)
{
    return (const void**)((char*) object + operations_field_offset);
}

/*
 *  State of the interceptor.
 * 
 * Useful for selfchecking and for ignore some operations in incorrect state.
 */
enum interceptor_state
{
	interceptor_state_uninitialized = 0,
	interceptor_state_initialized,
	interceptor_state_started,
};

//********** Interceptor for objects with its own operations **********

// Forward declarations
struct kedr_coi_interceptor;
struct kedr_coi_factory_interceptor;

// Internal API of operations interceptor(forward declaration)
static int factory_interceptor_start(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    struct kedr_coi_interceptor* interceptor_binded);

static void factory_interceptor_stop(
    struct kedr_coi_factory_interceptor* factory_interceptor);

/*
 * Interceptor structure.
 */
struct kedr_coi_interceptor
{
    // Current state of the interceptor
    int state;

    /*
     * Instrumentor used for instrument objects' operations.
     * 
     * Used only in 'started' state.
     */
    struct kedr_coi_instrumentor* instrumentor;

    /* 
     * Offset of the operations structure in the object.
     * For direct interceptor this field is '-1'.
     */
    size_t operations_field_offset;
    
    struct operation_payloads payloads;
    /*
     *  Protect list of foreign interceptors from concurrent access.
     */
    struct mutex m;

    // List of factory interceptors created by this one
    struct list_head factory_interceptors;

    size_t operations_struct_size;
    bool (*replace_at_place)(const void* ops);
    void (*trace_unforgotten_object)(const void* object);
    const char* name;    
};

//*************** Factory interceptor ********************************//

struct kedr_coi_factory_interceptor
{
    // Current state of the interceptor
    int state;

    /*
     * Instrumentor for this interceptor. 
     * Used only in 'started' state.
     */
    struct kedr_coi_foreign_instrumentor* instrumentor;

    /* 
     * Operations field offset in the factory object.
     * -1 for 'generic' factory interceptor.
     */
    size_t factory_operations_field_offset;

    struct operation_payloads payloads;
    
    struct kedr_coi_interceptor* interceptor_binded;

    // Element of the list of foreign interceptor binded with normal one.
    struct list_head list;

    void (*trace_unforgotten_object)(const void* object);
    const char* name;    
};

//*********Internal factory interceptor API implementation************//
int factory_interceptor_start(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    struct kedr_coi_interceptor* interceptor_binded)
{
	int result;
    struct kedr_coi_instrumentor* instrumentor_binded;
    const struct kedr_coi_replacement* replacements;

    //pr_info("Start factory interceptor: %s", factory_interceptor->name);
    
    BUG_ON(factory_interceptor->state != interceptor_state_initialized);
    BUG_ON(interceptor_binded->state != interceptor_state_started);

    result = operation_payloads_use(&factory_interceptor->payloads, 1);
    if(result) return result;

    replacements = operation_payloads_get_replacements(
        &factory_interceptor->payloads);
    
    instrumentor_binded = interceptor_binded->instrumentor;
    
    factory_interceptor->instrumentor = kedr_coi_foreign_instrumentor_create(
        instrumentor_binded, replacements);
    
    if(factory_interceptor->instrumentor == NULL)
    {
        operation_payloads_unuse(&factory_interceptor->payloads);
        return -ENOMEM;
    }
    
    factory_interceptor->state = interceptor_state_started;
    
    return 0;
}

static void factory_interceptor_trace_unforgotten_object(
    const void* object, void* user_data)
{
    struct kedr_coi_factory_interceptor* factory_interceptor = user_data;
    if(factory_interceptor->trace_unforgotten_object)
    {
        factory_interceptor->trace_unforgotten_object(object);
    }
#ifdef KEDR_COI_DEBUG
    else
    {
        pr_info("Object %p wasn't forgotten by factory interceptor %s.",
            object, factory_interceptor->name);
    }
#endif

}

void factory_interceptor_stop(
    struct kedr_coi_factory_interceptor* factory_interceptor)
{
    if(factory_interceptor->state == interceptor_state_initialized) return;
    
    BUG_ON(factory_interceptor->state != interceptor_state_started);

    factory_interceptor->state = interceptor_state_initialized;
    
    kedr_coi_foreign_instrumentor_destroy(
        factory_interceptor->instrumentor,
        factory_interceptor_trace_unforgotten_object,
        factory_interceptor);

    factory_interceptor->instrumentor = NULL;
    
    operation_payloads_unuse(&factory_interceptor->payloads);
}

//*********Interceptor API implementation*****************************//
/* 'replace_at_place' callback used for direct interceptor. */
static bool replace_at_place_always(const void* ops)
{
    (void)ops;
    return 1;
}
// Creation of the interceptor(common variant)
static struct kedr_coi_interceptor*
kedr_coi_interceptor_create_common(const char* name,
    size_t operations_struct_size,
    const struct kedr_coi_intermediate* intermediate_operations)
{
    int err;
    struct kedr_coi_interceptor* interceptor;
    
    interceptor = kzalloc(sizeof(*interceptor), GFP_KERNEL);
    
    if(interceptor == NULL)
    {
        pr_err("Cannot allocate operations interceptor.");
        return NULL;
    }
    
    err = operation_payloads_init(&interceptor->payloads,
        intermediate_operations, name);
    
    if(err) goto fail_payloads;
    
    interceptor->name = name;
    
    interceptor->operations_struct_size = operations_struct_size;
    
    interceptor->state = interceptor_state_initialized;
    
    interceptor->trace_unforgotten_object = NULL;
    
    mutex_init(&interceptor->m);

    INIT_LIST_HEAD(&interceptor->factory_interceptors);
    
    return interceptor;

fail_payloads:
    kfree(interceptor);
    return NULL;
}

struct kedr_coi_interceptor* kedr_coi_interceptor_create(
    const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_intermediate* intermediate_operations)
{
    struct kedr_coi_interceptor* interceptor =
        kedr_coi_interceptor_create_common(
            name,
            operations_struct_size,
            intermediate_operations);
    
    if(interceptor)
    {
        interceptor->operations_field_offset = operations_field_offset;
        interceptor->replace_at_place = &kedr_coi_is_module_address;
    }
    
    return interceptor;        
}

struct kedr_coi_interceptor* kedr_coi_interceptor_create_direct(
    const char* name,
    size_t object_size,
    const struct kedr_coi_intermediate* intermediate_operations)
{
    struct kedr_coi_interceptor* interceptor =
        kedr_coi_interceptor_create_common(
            name,
            object_size,
            intermediate_operations);
    
    if(interceptor)
    {
        interceptor->operations_field_offset = -1;
        interceptor->replace_at_place = &replace_at_place_always;
    }
    
    return interceptor;        
}


int kedr_coi_interceptor_start(struct kedr_coi_interceptor* interceptor)
{
	int result;
    struct kedr_coi_factory_interceptor* factory_interceptor;
    const struct kedr_coi_replacement* replacements;
    
	BUG_ON(interceptor->state != interceptor_state_initialized);

    result = operation_payloads_use(&interceptor->payloads, 0);
    if(result) return result;

    replacements = operation_payloads_get_replacements(
        &interceptor->payloads);

    interceptor->instrumentor = kedr_coi_instrumentor_create(
        interceptor->operations_struct_size,
        replacements,
        interceptor->replace_at_place);
    if(interceptor->instrumentor == NULL)
    {
        result = -ENOMEM;
        goto err_create_instrumentor;
    }
    
    interceptor->state = interceptor_state_started;
    // Also start all foreign interceptors created for this one.
    list_for_each_entry(factory_interceptor, &interceptor->factory_interceptors, list)
    {
        result = factory_interceptor_start(factory_interceptor, interceptor);
        if(result)
        {
            goto err_foreign_start;
        }
    }
    
    return 0;

err_foreign_start:
    list_for_each_entry_continue_reverse(factory_interceptor,
        &interceptor->factory_interceptors, list)
    {
        factory_interceptor_stop(factory_interceptor);
    }
    
    interceptor->state = interceptor_state_initialized;

    kedr_coi_instrumentor_destroy(interceptor->instrumentor,
        NULL, NULL);

    interceptor->instrumentor = NULL;

err_create_instrumentor:
    
    operation_payloads_unuse(&interceptor->payloads);
    
    return result;
}

static void interceptor_trace_unforgotten_watch(const void* object,
    void* user_data)
{
    struct kedr_coi_interceptor* interceptor = user_data;
    if(interceptor->trace_unforgotten_object)
        interceptor->trace_unforgotten_object(object);
#ifdef KEDR_COI_DEBUG
    else
        pr_info("Object %p wasn't forgotten for interceptor %s.",
            object, interceptor->name);
#endif
}

void kedr_coi_interceptor_stop(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_factory_interceptor* factory_interceptor;

	BUG_ON(interceptor->state == interceptor_state_uninitialized);
	
	if(interceptor->state == interceptor_state_initialized)
		return;// Assume that start() was called but failed

    // First - stop all foreign interceptors created for this one
    list_for_each_entry(factory_interceptor, &interceptor->factory_interceptors, list)
    {
        factory_interceptor_stop(factory_interceptor);
    }

    interceptor->state = interceptor_state_initialized;

    kedr_coi_instrumentor_destroy(interceptor->instrumentor,
        interceptor_trace_unforgotten_watch,
        interceptor);

    interceptor->instrumentor = NULL;
    
    operation_payloads_unuse(&interceptor->payloads);
}

int kedr_coi_interceptor_watch(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
    if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);
	
    if(interceptor->operations_field_offset != -1)
    {
        const void** ops_p = indirect_operations_p(object, interceptor->operations_field_offset);
        return kedr_coi_instrumentor_watch(
            interceptor->instrumentor,
            object,
            ops_p);
    }
    else
    {
        return kedr_coi_instrumentor_watch_direct(
            interceptor->instrumentor,
            object);
    }
}

int kedr_coi_interceptor_forget(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);

    if(interceptor->operations_field_offset != -1)
    {
        return kedr_coi_instrumentor_forget(
            interceptor->instrumentor,
            object,
            indirect_operations_p(object, interceptor->operations_field_offset));
    }
    else
    {
        return kedr_coi_instrumentor_forget_direct(
            interceptor->instrumentor,
            object,
            0);
    }
}

int kedr_coi_interceptor_forget_norestore(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);

    if(interceptor->operations_field_offset != -1)
    {
        return kedr_coi_instrumentor_forget(
            interceptor->instrumentor,
            object,
            NULL);
    }
    else
    {
        return kedr_coi_instrumentor_forget_direct(
            interceptor->instrumentor,
            object,
            1);
    }
}

int kedr_coi_payload_register(
	struct kedr_coi_interceptor* interceptor,
	struct kedr_coi_payload* payload)
{
    BUG_ON((interceptor->state != interceptor_state_initialized)
        && (interceptor->state != interceptor_state_started));
    
    if(interceptor->state == interceptor_state_started)
    {
        pr_err("Cannot register payload because interceptor is "
            "started. Please, stop it.");
        return -EBUSY;
    }

    return operation_payloads_add(&interceptor->payloads, payload);
}

int kedr_coi_payload_unregister(
	struct kedr_coi_interceptor* interceptor,
	struct kedr_coi_payload* payload)
{
	BUG_ON((interceptor->state != interceptor_state_initialized)
		&& (interceptor->state != interceptor_state_started));

    return operation_payloads_remove(&interceptor->payloads, payload);
}


int kedr_coi_interceptor_get_intermediate_info(
	struct kedr_coi_interceptor* interceptor,
	const void* object,
	size_t operation_offset,
	struct kedr_coi_intermediate_info* info)
{
    int result;
    const void* ops;
    
    /*
     * This function is allowed to be called only by intermediate operation.
     * Intermediate operation may be called only after successfull 
     * 'watch' call, which in turn may be only in 'started' state of
     * the interceptor.
     */
	BUG_ON(interceptor->state != interceptor_state_started);
    
    if(interceptor->operations_field_offset != -1)
    {
        ops = indirect_operations(object, interceptor->operations_field_offset);
    }
    else
    {
        ops = object;
    }

    result = kedr_coi_instrumentor_get_orig_operation(
        interceptor->instrumentor,
        object,
        ops,
        operation_offset,
        &info->op_orig);

    
    if(result == 0)
    {
        operation_payloads_get_interception_info(&interceptor->payloads,
            operation_offset, info->op_orig? 0 : 1,
            &info->pre, &info->post);

        return 0;
    }
    else if((result == 1)
        || ((result < 0) && !IS_ERR(info->op_orig)))
    {
        // without handlers - as if no interception at all.
        info->pre = NULL;
        info->post = NULL;

        return 0;
    }
    // Error
    return result;
}


void kedr_coi_interceptor_destroy(
	struct kedr_coi_interceptor* interceptor)
{
    BUG_ON(interceptor->state != interceptor_state_initialized);
	/* 
     * First - disconnect all foreign interceptor which are created
     * for this one.
     */
	while(!list_empty(&interceptor->factory_interceptors))
    {
        struct kedr_coi_factory_interceptor* factory_interceptor = list_first_entry(
            &interceptor->factory_interceptors, typeof(*factory_interceptor), list);
        
        factory_interceptor->interceptor_binded = NULL;
        list_del_init(&factory_interceptor->list);
    }

    operation_payloads_destroy(&interceptor->payloads);

    /*
     *  For control that nobody will access interceptor
     * when it has destroyed.
     */
	interceptor->state = interceptor_state_uninitialized;
    
    kfree(interceptor);
}

//*********Factory interceptor API implementation*********************//
static struct kedr_coi_factory_interceptor*
kedr_coi_factory_interceptor_create_common(
    struct kedr_coi_interceptor* interceptor,
    const char* name,
    const struct kedr_coi_intermediate* intermediate_operations)
{
    struct kedr_coi_factory_interceptor* factory_interceptor;
    int err;
    
    if(interceptor->operations_field_offset == -1)
    {
        pr_err("Cannot create factory interceptor for direct normal one.");
        return NULL;
    }

    BUG_ON(interceptor->state != interceptor_state_initialized);
    
    if((intermediate_operations == NULL) ||
        (intermediate_operations[0].operation_offset == -1))
    {
        pr_err("For factory interceptor at least one "
            "intermediate should be defined.");
        return NULL;
    }

    factory_interceptor = kmalloc(sizeof(*factory_interceptor), GFP_KERNEL);
    if(!factory_interceptor) return NULL;

    err = operation_payloads_init(&factory_interceptor->payloads,
        intermediate_operations, name);
    
    if(err) goto fail_payloads;
    
    factory_interceptor->name = name;
    factory_interceptor->instrumentor = NULL;

    factory_interceptor->state = interceptor_state_initialized;
    factory_interceptor->trace_unforgotten_object = NULL;

    factory_interceptor->interceptor_binded = interceptor;
    list_add_tail(&factory_interceptor->list, &interceptor->factory_interceptors);

    return factory_interceptor;

fail_payloads:
    kfree(factory_interceptor);
    return NULL;
}

struct kedr_coi_factory_interceptor* kedr_coi_factory_interceptor_create(
    struct kedr_coi_interceptor* interceptor,
    const char* name,
    size_t factory_operations_field_offset,
    const struct kedr_coi_intermediate* intermediate_operations)
{
    struct kedr_coi_factory_interceptor* factory_interceptor =
        kedr_coi_factory_interceptor_create_common(
            interceptor,
            name,
            intermediate_operations);
    
    if(factory_interceptor)
    {
        factory_interceptor->factory_operations_field_offset = factory_operations_field_offset;
    }
    
    return factory_interceptor;
}
            
struct kedr_coi_factory_interceptor* kedr_coi_factory_interceptor_create_generic(
    struct kedr_coi_interceptor* interceptor,
    const char* name,
    const struct kedr_coi_intermediate* intermediate_operations)
{
    struct kedr_coi_factory_interceptor* factory_interceptor =
        kedr_coi_factory_interceptor_create_common(
            interceptor,
            name,
            intermediate_operations);
    
    if(factory_interceptor)
    {
        factory_interceptor->factory_operations_field_offset = -1;
    }
    
    return factory_interceptor;
}


int kedr_coi_factory_interceptor_watch_generic(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* id,
    void* tie,
    const void** ops_p)
{
    if(factory_interceptor->state == interceptor_state_initialized)
    {
        return -EPERM;
    }
    
    BUG_ON(factory_interceptor->state != interceptor_state_started);
    
    return kedr_coi_foreign_instrumentor_watch(factory_interceptor->instrumentor,
        id, tie, ops_p);
}

/* Normal variant*/
int kedr_coi_factory_interceptor_watch(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* factory)
{
    void* id = factory;
    void* tie = factory;
    const void** ops_p;
    
    if(factory_interceptor->factory_operations_field_offset == -1)
    {
        pr_err("kedr_coi_factory_interceptor_watch() shouldn't be used for generic factory interceptor. Use '_generic' variant.");
        return -EINVAL;
    }
    
    ops_p = indirect_operations_p(factory,
        factory_interceptor->factory_operations_field_offset);
    
    return kedr_coi_factory_interceptor_watch_generic(
        factory_interceptor, id, tie, ops_p);
}

int kedr_coi_factory_interceptor_forget_generic(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* id,
    const void** ops_p)
{
    if(factory_interceptor->state == interceptor_state_initialized)
        return -EPERM;
    
    BUG_ON(factory_interceptor->state != interceptor_state_started);
    
    return kedr_coi_foreign_instrumentor_forget(
        factory_interceptor->instrumentor, id, ops_p);
}

/* Normal variants */
int kedr_coi_factory_interceptor_forget(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* factory)
{
    void* id = factory;
    const void** ops_p;
    
    if(factory_interceptor->factory_operations_field_offset == -1)
    {
        pr_err("kedr_coi_factory_interceptor_forget() shouldn't be used for generic factory interceptor. Use '_generic' variant.");
        return -EINVAL;
    }
    
    ops_p = indirect_operations_p(factory,
        factory_interceptor->factory_operations_field_offset);
    
    return kedr_coi_factory_interceptor_forget_generic(
        factory_interceptor, id, ops_p);
}

int kedr_coi_factory_interceptor_forget_norestore(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* factory)
{
    void* id = factory;

    return kedr_coi_factory_interceptor_forget_generic(
        factory_interceptor, id, NULL);
}


int kedr_coi_factory_interceptor_bind_object(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* object,
    const void* factory,
    size_t operation_offset,
    struct kedr_coi_intermediate_info* info)
{
    struct kedr_coi_interceptor* interceptor_binded =
        factory_interceptor->interceptor_binded;
        
    const void** ops_p;

    /*
     * Similar to  kedr_coi_get_intermediate_info, this function
     * cannot be called if interceptor is not started.
     */
    BUG_ON(factory_interceptor->state != interceptor_state_started);

    ops_p = indirect_operations_p(object,
        interceptor_binded->operations_field_offset);

    //TODO: Fill handlers
    info->pre = NULL;
    info->post = NULL;
    
    return kedr_coi_foreign_instrumentor_bind(
        factory_interceptor->instrumentor,
        object,
        factory,
        ops_p,
        operation_offset,
        &info->op_chained,
        &info->op_orig);
}

void kedr_coi_factory_interceptor_destroy(
    struct kedr_coi_factory_interceptor* factory_interceptor)
{
    BUG_ON(factory_interceptor->state != interceptor_state_initialized);
    factory_interceptor->state = interceptor_state_uninitialized;

    if(!list_empty(&factory_interceptor->list))
    {
        list_del(&factory_interceptor->list);
    }
    
    operation_payloads_destroy(&factory_interceptor->payloads);
    
    kfree(factory_interceptor);
}

/*********** Methods which affects on interceptor's behaviour**********/
void kedr_coi_interceptor_trace_unforgotten_object(
    struct kedr_coi_interceptor* interceptor,
    void (*cb)(const void* object))
{
    interceptor->trace_unforgotten_object = cb;
}

void kedr_coi_factory_interceptor_trace_unforgotten_object(
    struct kedr_coi_factory_interceptor* interceptor,
    void (*cb)(const void* object))
{
    interceptor->trace_unforgotten_object = cb;
}


bool kedr_coi_is_module_address(const void* addr)
{
    struct module* m = __module_address((unsigned long)addr);
    return !m;
}

void kedr_coi_interceptor_mechanism_selector(
    struct kedr_coi_interceptor* interceptor,
    bool (*replace_at_place)(const void* ops))
{
    if(!replace_at_place)
    {
        pr_err("kedr_coi_interceptor_mechanism_selector() shouldn't be called with NULL callback.");
        return;

    }
    if(interceptor->operations_field_offset == -1)
    {
        pr_err("kedr_coi_interceptor_mechanism_selector() shouldn't be called for direct interceptor.");
        return;
    }
    
    interceptor->replace_at_place = replace_at_place;
}
