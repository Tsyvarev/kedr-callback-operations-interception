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

#include <linux/slab.h>

#include "payloads.h"

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
 * Operations which differs in interceptors of different types.
 */
struct interceptor_operations
{
    /*
     * Return instrumentor for work with given replacements.
     */
    struct kedr_coi_instrumentor* (*create_instrumentor)(
        struct kedr_coi_interceptor* interceptor,
        size_t operations_struct_size,
        const struct kedr_coi_replacement* replacements);
    // Free instance of the interceptor
    void (*destroy)(struct kedr_coi_interceptor* interceptor);
    
    /*
     * Create instrumentor with foreign support.
     * 
     * This method is used when one or more foreign interceptors is
     * created for this one.
     * Also this method is used instead of 'create_instrumentor', if
     * latest is NULL.
     */
    struct kedr_coi_instrumentor_with_foreign*
    (*create_instrumentor_with_foreign)(
        struct kedr_coi_interceptor* interceptor,
        size_t operations_struct_size,
        const struct kedr_coi_replacement* replacements);
};

/*
 * Base interceptor structure.
 */
struct kedr_coi_interceptor
{
    const char* name;
    const struct interceptor_operations* ops;
    
    size_t operations_struct_size;
    size_t operations_field_offset;// -1 for direct interceptor
    
    void (*trace_unforgotten_object)(void* object);
    // Current state of the interceptor
    int state;

    struct operation_payloads payloads;
    // List of factory interceptors created by this one
    struct list_head factory_interceptors;
    /*
     *  Protect list of foreign interceptors from concurrent access.
     */
    struct mutex m;
    
    // Next fields are used only when start to intercept.
    
    // Instrumentor used for instrument objects' operations.
    struct kedr_coi_instrumentor* instrumentor;
};

static void interceptor_trace_unforgotten_watch(void* object,
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

/*
 * Return pointer to the object's operations
 */
static const void* interceptor_get_ops(
    struct kedr_coi_interceptor* interceptor, const void* object)
{
    return (interceptor->operations_field_offset == -1)
        ? object
        : indirect_operations(object, interceptor->operations_field_offset);
}


/*
 *  Return reference to the pointer of object's operations,
 * depending of interceptor type(direct or indirect).
 * 
 * NOTE: 'object' should be variable, because for direct interceptor
 * reference to this variable will be returned. This is the reason why
 * interceptor_get_ops_p() is a not a function but a macro.
 */
#define interceptor_get_ops_p(interceptor, object) ((interceptor->operations_field_offset == -1) \
    ? (const void**)&object \
    : indirect_operations_p(object, interceptor->operations_field_offset))



/*
 * Initialize fields of the base interceptor structure.
 */
int
interceptor_init_common(
    struct kedr_coi_interceptor* interceptor,
    const char* name,
    const struct interceptor_operations* ops,
    size_t operations_struct_size,
    size_t operations_field_offset,
    struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object))
{
    int result;

    result = operation_payloads_init(&interceptor->payloads,
        intermediate_operations, name);
    
    if(result) return result;
    
    interceptor->name = name;
    interceptor->ops = ops;
    
    interceptor->operations_struct_size = operations_struct_size;
    interceptor->operations_field_offset = operations_field_offset;
    
    interceptor->state = interceptor_state_initialized;
    
    interceptor->trace_unforgotten_object = trace_unforgotten_object;
    
    mutex_init(&interceptor->m);

    INIT_LIST_HEAD(&interceptor->factory_interceptors);
    
    return 0;
}


//*************** Factory interceptor ********************************//

/* 
 * Operations which differs in factory interceptors of different types.
 */
struct factory_interceptor_operations
{
    // Free instance of the interceptor
    void (*destroy)(
        struct kedr_coi_factory_interceptor* factory_interceptor);

    void (*trace_unforgotten_watch)(
        struct kedr_coi_factory_interceptor* factory_interceptor,
        void* id, void* tie);
    
    // Return operations pointer for given factory object.
    // For generic interceptor should be NULL.
    const void** (*get_ops_p)(
        struct kedr_coi_factory_interceptor* factory_interceptor,
        void* factory);

};


struct kedr_coi_factory_interceptor
{
    const char* name;
    const struct factory_interceptor_operations* ops;

    struct kedr_coi_interceptor* interceptor_binded;
    // Current state of the interceptor
    int state;
    

    struct operation_payloads payloads;    
    // Element of the list of foreign interceptor binded with normal one.
    struct list_head list;
    
    /*
     * Instrumentor for this interceptor. 
     *  Used only when interceptor is started.
     */
    struct kedr_coi_foreign_instrumentor* instrumentor;
};

static void kedr_coi_factory_interceptor_trace_unforgotten_watch(
    void* id, void* tie, void* user_data)
{
    struct kedr_coi_factory_interceptor* factory_interceptor = user_data;
    factory_interceptor->ops->trace_unforgotten_watch(
        factory_interceptor, id, tie);
}

/* 
 * Non-generic functions for factory interceptor use this functions
 * for deduce operations pointer from 'factory' pointer.
 */
static const void** factory_interceptor_get_ops_p(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* factory)
{
    const struct factory_interceptor_operations* ops
        = factory_interceptor->ops;
    
    /* Non-generic functions is forbidden for generic interceptor */
    BUG_ON(ops->get_ops_p == NULL);
    
    return ops->get_ops_p(factory_interceptor, factory);
}

/*
 *  Initialize factory interceptor structure as not binded.
 */
static int factory_interceptor_init_common(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    const char* name,
    const struct factory_interceptor_operations* ops,
    const struct kedr_coi_intermediate* intermediate_operations)
{
    int result;
    
    const struct kedr_coi_intermediate* intermediate;
    /* 
     * Check that no intermediate is declared as 'internal_only'.
     */
    if((intermediate_operations == NULL) ||
        (intermediate_operations[0].operation_offset == -1))
    {
        pr_err("For factory interceptor at least one "
            "intermediate should be defined.");
        return -EINVAL;
    }
        
    
    for(intermediate = intermediate_operations;
        intermediate->operation_offset != -1;
        intermediate++)
    {
        if(intermediate->internal_only)
        {
            pr_err("For factory interceptor internal-only "
                "intermediates are prohibited.");
            return -EINVAL;
        }
    }
    
    result = operation_payloads_init(&factory_interceptor->payloads,
        intermediate_operations, name);
    
    if(result) return result;

    factory_interceptor->interceptor_binded = NULL;
    factory_interceptor->name = name;
    factory_interceptor->instrumentor = NULL;

    INIT_LIST_HEAD(&factory_interceptor->list);
    
    factory_interceptor->state = interceptor_state_initialized;
    
    return 0;
}

static int factory_interceptor_bind(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    struct kedr_coi_interceptor* interceptor)
{
    BUG_ON(interceptor->state != interceptor_state_initialized);
    
    if(interceptor->operations_field_offset == -1)
    {
        pr_err("Direct interceptor doesn't support foreign interceptors.");
        return -EINVAL;
    }

    factory_interceptor->interceptor_binded = interceptor;
    list_add_tail(&factory_interceptor->list, &interceptor->factory_interceptors);
    
    return 0;
}

//*********Interceptor API implementation*****************************//

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

    if(list_empty(&interceptor->factory_interceptors)
        && (interceptor->ops->create_instrumentor != NULL))
    {
        /*
         *  Create normal instrumentor, because no factory ones has been
         * created for this one.
         */
        interceptor->instrumentor =
            interceptor->ops->create_instrumentor(
                interceptor,
                interceptor->operations_struct_size,
                replacements);
        
        if(interceptor->instrumentor == NULL)
        {
            result = -EINVAL;
            goto err_create_instrumentor;
        }
    }
    else
    {
        /*
         * Create instrumentor with foreign support because
         * one or more foreign interceptors was created for this one
         * or 'create_instrumentor' method is NULL.
         */
        struct kedr_coi_instrumentor_with_foreign* instrumentor_with_foreign =
            interceptor->ops->create_instrumentor_with_foreign(
                interceptor,
                interceptor->operations_struct_size,
                replacements);
        
        if(instrumentor_with_foreign == NULL)
        {
            result = -EINVAL;
            goto err_create_instrumentor;
        }
        interceptor->instrumentor = &instrumentor_with_foreign->base;
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

    if(interceptor->trace_unforgotten_object)
    {
        kedr_coi_instrumentor_destroy(interceptor->instrumentor,
            interceptor_trace_unforgotten_watch,
            interceptor);
    }
    else
    {
        kedr_coi_instrumentor_destroy(interceptor->instrumentor,
            NULL, NULL);
    }
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
	
    return kedr_coi_instrumentor_watch(interceptor->instrumentor,
        object, interceptor_get_ops_p(interceptor, object));
}

int kedr_coi_interceptor_forget(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);

    return kedr_coi_instrumentor_forget(interceptor->instrumentor,
        object, interceptor_get_ops_p(interceptor, object));
}

int kedr_coi_interceptor_forget_norestore(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);

    return kedr_coi_instrumentor_forget(interceptor->instrumentor,
        object, NULL);
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
    
    /*
     * This function is allowed to be called only by intermediate operation.
     * Intermediate operation may be called only after successfull 
     * 'watch' call, which in turn may be only in 'started' state of
     * the interceptor.
     */
	BUG_ON(interceptor->state != interceptor_state_started);
    
    result = kedr_coi_instrumentor_get_orig_operation(
        interceptor->instrumentor,
        object,
        interceptor_get_ops(interceptor, object),
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
        
        list_del(&factory_interceptor->list);
    }

    operation_payloads_destroy(&interceptor->payloads);

    /*
     *  For control that nobody will access interceptor
     * when it has destroyed.
     */
	interceptor->state = interceptor_state_uninitialized;
    
    interceptor->ops->destroy(interceptor);
}


//*********Internal factory interceptor API implementation************//
int factory_interceptor_start(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    struct kedr_coi_interceptor* interceptor_binded)
{
	int result;
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded;
    const struct kedr_coi_replacement* replacements;
    
    BUG_ON(factory_interceptor->state != interceptor_state_initialized);
    BUG_ON(interceptor_binded->state != interceptor_state_started);

    result = operation_payloads_use(&factory_interceptor->payloads, 1);
    if(result) return result;

    replacements = operation_payloads_get_replacements(
        &factory_interceptor->payloads);
    
    instrumentor_binded = container_of(interceptor_binded->instrumentor,
        typeof(*instrumentor_binded), base);
    
    factory_interceptor->instrumentor = kedr_coi_foreign_instrumentor_create(
        instrumentor_binded, replacements);
    
    if(factory_interceptor->instrumentor == NULL)
    {
        operation_payloads_unuse(&factory_interceptor->payloads);
        return -EINVAL;
    }
    
    factory_interceptor->state = interceptor_state_started;
    
    return 0;
}

void factory_interceptor_stop(
    struct kedr_coi_factory_interceptor* factory_interceptor)
{
    if(factory_interceptor->state == interceptor_state_initialized) return;
    
    BUG_ON(factory_interceptor->state != interceptor_state_started);

    factory_interceptor->state = interceptor_state_initialized;
    
    kedr_coi_foreign_instrumentor_destroy(
        factory_interceptor->instrumentor,
        kedr_coi_factory_interceptor_trace_unforgotten_watch,
        factory_interceptor);

    factory_interceptor->instrumentor = NULL;
    
    operation_payloads_unuse(&factory_interceptor->payloads);
}

//*********Factory interceptor API implementation*********************//
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
    const void** ops_p = factory_interceptor_get_ops_p(factory_interceptor,
        factory);
    
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
    
    return kedr_coi_foreign_instrumentor_forget(factory_interceptor->instrumentor,
        id, ops_p);
}

/* Normal variants */
int kedr_coi_factory_interceptor_forget(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* factory)
{
    void* id = factory;
    const void** ops_p = factory_interceptor_get_ops_p(factory_interceptor,
        factory);
    
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
    void* factory,
    size_t operation_offset,
    struct kedr_coi_intermediate_info* info)
{
    struct kedr_coi_interceptor* interceptor_binded =
        factory_interceptor->interceptor_binded;
        
    const void** ops_p = indirect_operations_p(object,
        interceptor_binded->operations_field_offset);
    /*
     * Similar to  kedr_coi_get_intermediate_info, this function
     * cannot be called if interceptor is not started.
     */
    BUG_ON(factory_interceptor->state != interceptor_state_started);
    
    //TODO: Fill handlers
    return kedr_coi_foreign_instrumentor_bind(
        factory_interceptor->instrumentor,
        object,
        factory,
        ops_p,
        operation_offset,
        info->op_chained,
        info->op_orig);
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
    
    factory_interceptor->ops->destroy(factory_interceptor);
}
/************** 'Normal' factory interceptor***************************/
struct kedr_coi_factory_interceptor_normal
{
    struct kedr_coi_factory_interceptor base;
    
    size_t operations_field_offset;
    void (*trace_unforgotten_object)(void* object);
};

void factory_interceptor_normal_ops_destroy(
    struct kedr_coi_factory_interceptor* factory_interceptor)
{
    struct kedr_coi_factory_interceptor_normal* interceptor_real =
        container_of(factory_interceptor, typeof(*interceptor_real), base);
    
    kfree(interceptor_real);
}

void factory_interceptor_normal_ops_trace_unforgotten_watch(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* id, void* tie)
{
    struct kedr_coi_factory_interceptor_normal* interceptor_real =
        container_of(factory_interceptor, typeof(*interceptor_real), base);

    if(interceptor_real->trace_unforgotten_object)
    {
        interceptor_real->trace_unforgotten_object(id);
    }
#if KEDR_COI_DEBUG
    else
    {
        pr_info("Object %p wasn't forgotten by factory interceptor %s.",
            id, factory_interceptor->name);
    }
#endif
}

const void** factory_interceptor_normal_ops_get_ops_p(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* factory)
{
    struct kedr_coi_factory_interceptor_normal* interceptor_real =
        container_of(factory_interceptor, typeof(*interceptor_real), base);

    return indirect_operations_p(factory,
        interceptor_real->operations_field_offset);
}

const struct factory_interceptor_operations normal_ops =
{
    .destroy = factory_interceptor_normal_ops_destroy,
    .trace_unforgotten_watch = factory_interceptor_normal_ops_trace_unforgotten_watch,
    .get_ops_p = factory_interceptor_normal_ops_get_ops_p,
};

struct kedr_coi_factory_interceptor*
kedr_coi_factory_interceptor_create(
    struct kedr_coi_interceptor* interceptor_indirect,
    const char* name,
    size_t factory_operations_field_offset,
    const struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object))
{
    int result;
    
    struct kedr_coi_factory_interceptor_normal* interceptor_normal =
        kmalloc(sizeof(interceptor_normal), GFP_KERNEL);
    if(interceptor_normal == NULL)
    {
        return NULL;
    }
    
    result = factory_interceptor_init_common(
        &interceptor_normal->base,
        name,
        &normal_ops,
        intermediate_operations);
    
    if(result)
    {
        kfree(interceptor_normal);
        return NULL;
    }
    
    interceptor_normal->operations_field_offset = factory_operations_field_offset;
    interceptor_normal->trace_unforgotten_object = trace_unforgotten_object;

    result = factory_interceptor_bind(&interceptor_normal->base,
        interceptor_indirect);
    if(result)
    {
        kedr_coi_factory_interceptor_destroy(&interceptor_normal->base);
        return NULL;
    }
    
    return &interceptor_normal->base;
}

/************** 'Generic' factory interceptor**************************/
struct kedr_coi_factory_interceptor_generic
{
    struct kedr_coi_factory_interceptor base;
    
    void (*trace_unforgotten_ops)(void* id, void* tie);
};

void factory_interceptor_generic_ops_destroy(
    struct kedr_coi_factory_interceptor* factory_interceptor)
{
    struct kedr_coi_factory_interceptor_generic* interceptor_real =
        container_of(factory_interceptor, typeof(*interceptor_real), base);
    
    kfree(interceptor_real);
}

void factory_interceptor_generic_ops_trace_unforgotten_watch(
    struct kedr_coi_factory_interceptor* factory_interceptor,
    void* id, void* tie)
{
    struct kedr_coi_factory_interceptor_generic* interceptor_real =
        container_of(factory_interceptor, typeof(*interceptor_real), base);

    if(interceptor_real->trace_unforgotten_ops)
    {
        interceptor_real->trace_unforgotten_ops(id, tie);
    }
#if KEDR_COI_DEBUG
    else
    {
        pr_info("Object %p (with tie %p) wasn't forgotten by factory interceptor %s.",
            id, tie, factory_interceptor->name);
    }
#endif
}


const struct factory_interceptor_operations generic_ops =
{
    .destroy = factory_interceptor_generic_ops_destroy,
    .trace_unforgotten_watch = factory_interceptor_generic_ops_trace_unforgotten_watch,
};

struct kedr_coi_factory_interceptor*
kedr_coi_factory_interceptor_create_generic(
    struct kedr_coi_interceptor* interceptor_indirect,
    const char* name,
    const struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_ops)(void* id, void* tie))
{
    int result;
    
    struct kedr_coi_factory_interceptor_generic* interceptor_generic =
        kmalloc(sizeof(interceptor_generic), GFP_KERNEL);
    if(interceptor_generic == NULL)
    {
        return NULL;
    }
    
    result = factory_interceptor_init_common(
        &interceptor_generic->base,
        name,
        &generic_ops,
        intermediate_operations);
    
    if(result)
    {
        kfree(interceptor_generic);
        return NULL;
    }
    
    interceptor_generic->trace_unforgotten_ops = trace_unforgotten_ops;

    result = factory_interceptor_bind(&interceptor_generic->base,
        interceptor_indirect);
    if(result)
    {
        kedr_coi_factory_interceptor_destroy(&interceptor_generic->base);
        return NULL;
    }
   
    return &interceptor_generic->base;
}

/**********************************************************************/
/*******************Interceptors of concrete types*********************/
/**********************************************************************/

/*
 * Interceptor for objects with indirect operations(mostly used).
 */

/*
 * Indirect interceptor structure.
 */
struct kedr_coi_interceptor_indirect
{
    struct kedr_coi_interceptor base;
    
    /*
     * Do not replace operations at place;
     * copy operations structure and replace operations there.
     */
    bool use_copy;
};

#define interceptor_indirect(interceptor) container_of(interceptor, struct kedr_coi_interceptor_indirect, base)

//************Operations of the indirect interceptor******************//

static struct kedr_coi_instrumentor_with_foreign*
interceptor_indirect_op_create_instrumentor_with_foreign(
    struct kedr_coi_interceptor* interceptor,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_interceptor_indirect* interceptor_real =
        interceptor_indirect(interceptor);

    return interceptor_real->use_copy
        ? kedr_coi_instrumentor_with_foreign_create_indirect(
            operations_struct_size,
            replacements)
        : kedr_coi_instrumentor_with_foreign_create_indirect1(
            operations_struct_size,
            replacements);
}

static struct kedr_coi_instrumentor*
interceptor_indirect_op_create_instrumentor(
    struct kedr_coi_interceptor* interceptor,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_interceptor_indirect* interceptor_real =
        interceptor_indirect(interceptor);
    
    return interceptor_real->use_copy
        ? kedr_coi_instrumentor_create_indirect(
            operations_struct_size,
            replacements)
        : kedr_coi_instrumentor_create_indirect1(
            operations_struct_size,
            replacements);
}


static void
interceptor_indirect_op_destroy(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_interceptor_indirect* interceptor_real =
        interceptor_indirect(interceptor);
    
    kfree(interceptor_real);
}

//********Operations of the indirect interceptor**************//

static struct interceptor_operations interceptor_indirect_ops =
{
    .create_instrumentor = interceptor_indirect_op_create_instrumentor,
    .destroy = interceptor_indirect_op_destroy,
    
    .create_instrumentor_with_foreign = interceptor_indirect_op_create_instrumentor_with_foreign
};

// Creation of the indirect interceptor
static struct kedr_coi_interceptor*
kedr_coi_interceptor_create_common(const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object),
    bool use_copy)
{
    int result;
    struct kedr_coi_interceptor_indirect* interceptor;
    
    interceptor = kzalloc(sizeof(*interceptor), GFP_KERNEL);
    
    if(interceptor == NULL)
    {
        pr_err("Cannot allocate operations interceptor.");
        return NULL;
    }
    
    result = interceptor_init_common(&interceptor->base,
        name,
        &interceptor_indirect_ops,
        operations_struct_size,
        operations_field_offset,
        intermediate_operations,
        trace_unforgotten_object);
    
    if(result)
    {
        pr_err("Failed to initialize interceptor.");
        kfree(interceptor);
        
        return NULL;
    }

    interceptor->use_copy = use_copy;
    
    return &interceptor->base;
}

struct kedr_coi_interceptor*
kedr_coi_interceptor_create(const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object))
{
    return kedr_coi_interceptor_create_common(name, operations_field_offset,
        operations_struct_size, intermediate_operations,
        trace_unforgotten_object, false);
}

struct kedr_coi_interceptor*
kedr_coi_interceptor_create_use_copy(const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object))
{
    return kedr_coi_interceptor_create_common(name, operations_field_offset,
        operations_struct_size, intermediate_operations,
        trace_unforgotten_object, true);
}

//********Interceptor for objects with direct operations**************//

struct kedr_coi_interceptor_direct
{
    struct kedr_coi_interceptor base;
    
    size_t object_struct_size;
};

#define interceptor_direct(interceptor) container_of(interceptor, struct kedr_coi_interceptor_direct, base)


//*************Operations of the direct interceptor*******************//

static struct kedr_coi_instrumentor*
interceptor_direct_op_create_instrumentor(
    struct kedr_coi_interceptor* interceptor,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return kedr_coi_instrumentor_create_direct(
        operations_struct_size,
        replacements);
}

static void
interceptor_direct_op_destroy(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_interceptor_direct* interceptor_real =
        interceptor_direct(interceptor);
    
    kfree(interceptor_real);
}


static struct interceptor_operations interceptor_direct_ops =
{
    .create_instrumentor = interceptor_direct_op_create_instrumentor,
    .destroy = interceptor_direct_op_destroy
};

// Creation of the direct interceptor.
struct kedr_coi_interceptor*
kedr_coi_interceptor_create_direct(const char* name,
    size_t object_struct_size,
    struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object))
{
    int result;
    struct kedr_coi_interceptor_direct* interceptor;
    
    interceptor = kzalloc(sizeof(*interceptor), GFP_KERNEL);
    
    if(interceptor == NULL)
    {
        pr_err("Cannot allocate operations interceptor.");
        return NULL;
    }
    
    result = interceptor_init_common(&interceptor->base,
        name,
        &interceptor_direct_ops,
        object_struct_size,
        -1,
        intermediate_operations,
        trace_unforgotten_object);
    
    if(result)
    {
        pr_err("Failed to initialize interceptor.");
        kfree(interceptor);
        
        return NULL;
    }

    return &interceptor->base;
}
