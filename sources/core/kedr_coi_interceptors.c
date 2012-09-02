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
struct kedr_coi_foreign_core;

// Internal API of operations interceptor(forward declaration)
static int kedr_coi_foreign_core_start(struct kedr_coi_foreign_core* foreign_core,
    struct kedr_coi_interceptor* interceptor_binded);

static void kedr_coi_foreign_core_stop(struct kedr_coi_foreign_core* foreign_core);

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
    // List of foreign cores created by this one
    struct list_head foreign_cores;
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
 * reference to this variable will be returned. This is reason why
 * interceptor_get_ops_p() is a macro function but a function.
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
    
    interceptor->name = name;
    interceptor->ops = ops;
    
    interceptor->operations_struct_size = operations_struct_size;
    interceptor->operations_field_offset = operations_field_offset;
    
    interceptor->state = interceptor_state_initialized;
    
    interceptor->trace_unforgotten_object = trace_unforgotten_object;
    
    mutex_init(&interceptor->m);

    INIT_LIST_HEAD(&interceptor->foreign_cores);

    result = operation_payloads_init(&interceptor->payloads,
        intermediate_operations, name);
    
    if(result) return result;
    
    return 0;
}


//***************Interceptor for foreign operations*******************//

/*
 * Common part for kedr_coi_creation_interceptor and
 * kedr_coi_factory_interceptor.
 * 
 * Used for communicating with binded interceptor.
 */
struct kedr_coi_foreign_core
{
    struct kedr_coi_interceptor* interceptor_binded;
    // Current state of the interceptor
    int state;
    // Element of the list of foreign interceptor binded with normal one.
    struct list_head list;
    
    void (*trace_unforgotten_watch)(
        struct kedr_coi_foreign_core* foreign_core, void* id, void* tie);
    /*
     *  Replacements taken immediately from intermediate operations.
     * Used when interceptor is starting.
     */
    struct kedr_coi_replacement* replacements;
    /*
     * Instrumentor for this interceptor. 
     *  Used only when interceptor is started.
     */
    struct kedr_coi_foreign_instrumentor* instrumentor;
};

static void kedr_coi_foreign_core_trace_unforgotten_watch(void* id,
    void* tie, void* user_data)
{
    struct kedr_coi_foreign_core* foreign_core = user_data;
    foreign_core->trace_unforgotten_watch(foreign_core,
        (void*)id, (void*)tie);
}


/*
 *  Initialize foreign core structure as not binded.
 * 
 * 'replacements' will be controlled by foreign foreign_core, and freed when
 * no needed.
 */
static void kedr_coi_foreign_core_init(
    struct kedr_coi_foreign_core* foreign_core,
    struct kedr_coi_replacement* replacements,
    void (*trace_unforgotten_watch)(
        struct kedr_coi_foreign_core* foreign_core, void* id, void* tie))
{
    foreign_core->replacements = replacements;
    
    foreign_core->interceptor_binded = NULL;
    
    foreign_core->trace_unforgotten_watch = trace_unforgotten_watch;
    
    foreign_core->instrumentor = NULL;

    INIT_LIST_HEAD(&foreign_core->list);
    
    foreign_core->state = interceptor_state_initialized;
}

static int kedr_coi_foreign_core_bind(
    struct kedr_coi_foreign_core* foreign_core,
    struct kedr_coi_interceptor* interceptor)
{
    BUG_ON(interceptor->state != interceptor_state_initialized);
    
    if(interceptor->operations_field_offset == -1)
    {
        pr_err("Direct interceptor doesn't support foreign interceptors.");
        return -EINVAL;
    }

    foreign_core->interceptor_binded = interceptor;
    list_add_tail(&foreign_core->list, &interceptor->foreign_cores);
    
    return 0;
}

static void kedr_coi_foreign_core_finalize(
    struct kedr_coi_foreign_core* foreign_core)
{
    BUG_ON(foreign_core->state != interceptor_state_initialized);
    foreign_core->state = interceptor_state_uninitialized;

    if(!list_empty(&foreign_core->list))
    {
        list_del(&foreign_core->list);
    }
    
    kfree(foreign_core->replacements);
}


//*********Interceptor API implementation*****************************//

int kedr_coi_interceptor_start(struct kedr_coi_interceptor* interceptor)
{
	int result;
    struct kedr_coi_foreign_core* foreign_core;
    const struct kedr_coi_replacement* replacements;
    
	BUG_ON(interceptor->state != interceptor_state_initialized);

    result = operation_payloads_use(&interceptor->payloads, 0);
    if(result) return result;

    replacements = operation_payloads_get_replacements(
        &interceptor->payloads);

    if(list_empty(&interceptor->foreign_cores)
        && (interceptor->ops->create_instrumentor != NULL))
    {
        /*
         *  Create normal instrumentor, because no foreign interceptors
         * was created for this one.
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
    list_for_each_entry(foreign_core, &interceptor->foreign_cores, list)
    {
        result = kedr_coi_foreign_core_start(foreign_core, interceptor);
        if(result)
        {
            goto err_foreign_start;
        }
    }
    
    return 0;

err_foreign_start:
    list_for_each_entry_continue_reverse(foreign_core,
        &interceptor->foreign_cores, list)
    {
        kedr_coi_foreign_core_stop(foreign_core);
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
    struct kedr_coi_foreign_core* foreign_core;

	BUG_ON(interceptor->state == interceptor_state_uninitialized);
	
	if(interceptor->state == interceptor_state_initialized)
		return;// Assume that start() was called but failed

    // First - stop all foreign interceptors created for this one
    list_for_each_entry(foreign_core, &interceptor->foreign_cores, list)
    {
        kedr_coi_foreign_core_stop(foreign_core);
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
     * This function is allowed to call only by intermediate operation.
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
	while(!list_empty(&interceptor->foreign_cores))
    {
        struct kedr_coi_foreign_core* foreign_core = list_first_entry(
            &interceptor->foreign_cores, typeof(*foreign_core), list);
        
        list_del(&foreign_core->list);
    }

    operation_payloads_destroy(&interceptor->payloads);

    /*
     *  For control that nobody will access interceptor
     * when it has destroyed.
     */
	interceptor->state = interceptor_state_uninitialized;
    
    interceptor->ops->destroy(interceptor);
}


// Internal API of foreign core
int kedr_coi_foreign_core_start(
    struct kedr_coi_foreign_core* foreign_core,
    struct kedr_coi_interceptor* interceptor_binded)
{
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded;
    
    BUG_ON(foreign_core->state != interceptor_state_initialized);
    BUG_ON(interceptor_binded->state != interceptor_state_started);
    
    instrumentor_binded = container_of(interceptor_binded->instrumentor,
        typeof(*instrumentor_binded), base);
    
    foreign_core->instrumentor = kedr_coi_foreign_instrumentor_create(
        instrumentor_binded,
        foreign_core->replacements);
    
    if(foreign_core->instrumentor == NULL)
    {
        return -EINVAL;
    }
    
    foreign_core->state = interceptor_state_started;
    
    return 0;
}

void kedr_coi_foreign_core_stop(
    struct kedr_coi_foreign_core* foreign_core)
{
    if(foreign_core->state == interceptor_state_initialized) return;
    
    BUG_ON(foreign_core->state != interceptor_state_started);

    foreign_core->state = interceptor_state_initialized;
    
    if(foreign_core->trace_unforgotten_watch)
    {
        kedr_coi_foreign_instrumentor_destroy(
            foreign_core->instrumentor,
            kedr_coi_foreign_core_trace_unforgotten_watch,
            foreign_core);
    }
    else
    {
        kedr_coi_foreign_instrumentor_destroy(
            foreign_core->instrumentor,
            NULL, NULL);
    }

    foreign_core->instrumentor = NULL;
}


int kedr_coi_foreign_core_watch(
    struct kedr_coi_foreign_core* foreign_core,
    void* id,
    void* tie,
    const void** ops_p)
{
    if(foreign_core->state == interceptor_state_initialized)
    {
        return -EPERM;
    }
    
    BUG_ON(foreign_core->state != interceptor_state_started);
    
    return kedr_coi_foreign_instrumentor_watch(foreign_core->instrumentor,
        id, tie, ops_p);
}

int kedr_coi_foreign_core_forget(
    struct kedr_coi_foreign_core* foreign_core,
    void* id,
    const void** ops_p)
{
    if(foreign_core->state == interceptor_state_initialized)
        return -EPERM;
    
    BUG_ON(foreign_core->state != interceptor_state_started);
    
    return kedr_coi_foreign_instrumentor_forget(foreign_core->instrumentor,
        id, ops_p);
}

int kedr_coi_foreign_core_forget_norestore(
    struct kedr_coi_foreign_core* foreign_core,
    void* id)
{
    if(foreign_core->state == interceptor_state_initialized)
        return -EPERM;
    
    BUG_ON(foreign_core->state != interceptor_state_started);
    
    return kedr_coi_foreign_instrumentor_forget(foreign_core->instrumentor,
        id, NULL);
}

int kedr_coi_foreign_core_bind_object(
    struct kedr_coi_foreign_core* foreign_core,
    void* object,
    void* tie,
    size_t operation_offset,
    void** op_chained,
    void** op_orig/* for info only */)
{
    struct kedr_coi_interceptor* interceptor_binded =
        foreign_core->interceptor_binded;
        
    const void** ops_p = indirect_operations_p(object,
        interceptor_binded->operations_field_offset);
    /*
     * Similar to  kedr_coi_get_intermediate_info, this function
     * cannot be called if interceptor is not started.
     */
    BUG_ON(foreign_core->state != interceptor_state_started);
    
    return kedr_coi_foreign_instrumentor_bind(
        foreign_core->instrumentor,
        object,
        tie,
        ops_p,
        operation_offset,
        op_chained,
        op_orig);
}

/**********************************************************************/
/***********************Factory interceptor****************************/
/**********************************************************************/
struct kedr_coi_factory_interceptor
{
    struct kedr_coi_foreign_core foreign_core;
    
    const char* name;
    
    size_t operations_field_offset;
    
    void (*trace_unforgotten_object)(void* object);
};

void factory_interceptor_trace_unforgotten_watch(
    struct kedr_coi_foreign_core* foreign_core,
    void* id,
    void* tie)
{
    struct kedr_coi_factory_interceptor* interceptor = container_of(
        foreign_core, typeof(*interceptor), foreign_core);
    
    if(interceptor->trace_unforgotten_object)
        interceptor->trace_unforgotten_object(id);
#ifdef KEDR_COI_DEBUG
    else
        pr_info("Object %p wasn't forgotten for interceptor %s.",
            id, interceptor->name);
#endif
}

//*********Factory interceptor API implementation******************//
struct kedr_coi_factory_interceptor*
kedr_coi_factory_interceptor_create(
    struct kedr_coi_interceptor* interceptor_indirect,
    const char* name,
    size_t factory_operations_field_offset,
    const struct kedr_coi_factory_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object))
{
    struct kedr_coi_replacement* replacements;
    const struct kedr_coi_factory_intermediate* intermediate_operation;
    int n_intermediates;
    int i;

    struct kedr_coi_factory_interceptor* factory_interceptor =
        kmalloc(sizeof(*factory_interceptor), GFP_KERNEL);
    if(factory_interceptor == NULL)
    {
        pr_err("Failed to allocate foreign interceptor structure.");
        return NULL;
    }


    if((intermediate_operations == NULL)
        || (intermediate_operations[0].operation_offset == -1))
    {
        pr_err("Factory instrumentor cannot be created without intermediate operations.");
        goto intermediate_operations_err;
    }

    n_intermediates = 0;
    for(intermediate_operation = intermediate_operations;
        intermediate_operation->operation_offset != -1;
        intermediate_operation++)
    {
        n_intermediates ++;
    }
    
    replacements = kmalloc(sizeof(*replacements) * (n_intermediates + 1), GFP_KERNEL);
    if(replacements == NULL)
    {
        pr_err("Failed to allocate replacements for factory interceptor.");
        goto intermediate_operations_err;
    }
    
    i = 0;
    for(intermediate_operation = intermediate_operations;
        intermediate_operation->operation_offset != -1;
        intermediate_operation++)
    {
        replacements[i].operation_offset = intermediate_operation->operation_offset;
        replacements[i].repl = intermediate_operation->repl;
        replacements[i].mode = replace_all;
        i++;
    }
    BUG_ON(i != n_intermediates);
    replacements[n_intermediates].operation_offset = -1;

    factory_interceptor->name = name;
    factory_interceptor->operations_field_offset = factory_operations_field_offset;
    factory_interceptor->trace_unforgotten_object = trace_unforgotten_object;

    // Initialize foreign core structure
    kedr_coi_foreign_core_init(&factory_interceptor->foreign_core,
        replacements, factory_interceptor_trace_unforgotten_watch);
    
    // Bind interceptors
    if(kedr_coi_foreign_core_bind(&factory_interceptor->foreign_core,
        interceptor_indirect))
    {
        goto bind_err;
    }
    
    return factory_interceptor;

bind_err:
    kedr_coi_foreign_core_finalize(&factory_interceptor->foreign_core);
intermediate_operations_err:
    kfree(factory_interceptor);
    
    return NULL;
}

int kedr_coi_factory_interceptor_watch(
    struct kedr_coi_factory_interceptor* interceptor,
    void* factory)
{
    const void** ops_p = indirect_operations_p(factory,
        interceptor->operations_field_offset);
    
    return kedr_coi_foreign_core_watch(&interceptor->foreign_core,
        factory, factory, ops_p);
}

int kedr_coi_factory_interceptor_forget(
    struct kedr_coi_factory_interceptor* interceptor,
    void* factory)
{
    const void** ops_p = indirect_operations_p(factory,
        interceptor->operations_field_offset);

    return kedr_coi_foreign_core_forget(&interceptor->foreign_core,
        factory, ops_p);
}

int kedr_coi_factory_interceptor_forget_norestore(
    struct kedr_coi_factory_interceptor* interceptor,
    void* factory)
{
    return kedr_coi_foreign_core_forget_norestore(
        &interceptor->foreign_core, factory);
}

int kedr_coi_bind_object_with_factory(
    struct kedr_coi_factory_interceptor* interceptor,
    void* object,
    void* factory,
    size_t operation_offset,
    struct kedr_coi_factory_intermediate_info* info)
{
    //TODO: Fill handlers
    info->pre = NULL;
    info->post = NULL;
    return kedr_coi_foreign_core_bind_object(
        &interceptor->foreign_core,
        object,
        factory,
        operation_offset,
        &info->op_chained,
        &info->op_orig);
}


void kedr_coi_factory_interceptor_destroy(
    struct kedr_coi_factory_interceptor* interceptor)
{
    kedr_coi_foreign_core_finalize(&interceptor->foreign_core);
    kfree(interceptor);
}

/**********************************************************************/
/*******************Creation interceptor*********************/
/**********************************************************************/

struct kedr_coi_creation_interceptor
{
    struct kedr_coi_foreign_core foreign_core;
    
    const char* name;
    
    void (*trace_unforgotten_watch)(void* id, void* tie);
};

void creation_interceptor_trace_unforgotten_watch(
    struct kedr_coi_foreign_core* foreign_core,
    void* id,
    void* tie)
{
    struct kedr_coi_creation_interceptor* interceptor = container_of(
        foreign_core, typeof(*interceptor), foreign_core);
    
    if(interceptor->trace_unforgotten_watch)
        interceptor->trace_unforgotten_watch(id, tie);
#ifdef KEDR_COI_DEBUG
    else
        pr_info("Watch (%p, %p) wasn't forgotten for interceptor %s.",
            id, tie, interceptor->name);
#endif
}

//*********Creation interceptor API implementation******************//
struct kedr_coi_creation_interceptor*
kedr_coi_creation_interceptor_create(
    struct kedr_coi_interceptor* interceptor_indirect,
    const char* name,
    const struct kedr_coi_creation_intermediate* intermediate_operations,
    void (*trace_unforgotten_watch)(void* id, void* tie))
{
    struct kedr_coi_replacement* replacements;
    const struct kedr_coi_creation_intermediate* intermediate_operation;
    int n_intermediates;
    int i;

    struct kedr_coi_creation_interceptor* creation_interceptor =
        kmalloc(sizeof(*creation_interceptor), GFP_KERNEL);
    if(creation_interceptor == NULL)
    {
        pr_err("Failed to allocate creation interceptor structure.");
        return NULL;
    }


    if((intermediate_operations == NULL)
        || (intermediate_operations[0].operation_offset == -1))
    {
        pr_err("Creation instrumentor cannot be created without intermediate operations.");
        goto intermediate_operations_err;
    }

    n_intermediates = 0;
    for(intermediate_operation = intermediate_operations;
        intermediate_operation->operation_offset != -1;
        intermediate_operation++)
    {
        n_intermediates ++;
    }
    
    replacements = kmalloc(sizeof(*replacements) * (n_intermediates + 1), GFP_KERNEL);
    if(replacements == NULL)
    {
        pr_err("Failed to allocate replacements for creation interceptor.");
        goto intermediate_operations_err;
    }
    
    i = 0;
    for(intermediate_operation = intermediate_operations;
        intermediate_operation->operation_offset != -1;
        intermediate_operation++)
    {
        replacements[i].operation_offset = intermediate_operation->operation_offset;
        replacements[i].repl = intermediate_operation->repl;
        replacements[i].mode = replace_all;
        i++;
    }
    BUG_ON(i != n_intermediates);
    replacements[n_intermediates].operation_offset = -1;

    creation_interceptor->name = name;
    creation_interceptor->trace_unforgotten_watch = trace_unforgotten_watch;

    // Initialize foreign core structure
    kedr_coi_foreign_core_init(&creation_interceptor->foreign_core,
        replacements, creation_interceptor_trace_unforgotten_watch);
    
    // Bind interceptors
    if(kedr_coi_foreign_core_bind(&creation_interceptor->foreign_core,
        interceptor_indirect))
    {
        goto bind_err;
    }
    
    return creation_interceptor;

bind_err:
    kedr_coi_foreign_core_finalize(&creation_interceptor->foreign_core);
intermediate_operations_err:
    kfree(creation_interceptor);
    
    return NULL;
}

int kedr_coi_creation_interceptor_watch(
    struct kedr_coi_creation_interceptor* interceptor,
    void* id,
    void* tie,
    const void** ops_p)
{
    return kedr_coi_foreign_core_watch(&interceptor->foreign_core,
        id, tie, ops_p);
}

int kedr_coi_creation_interceptor_forget(
    struct kedr_coi_creation_interceptor* interceptor,
    void* id,
    const void** ops_p)
{
    return kedr_coi_foreign_core_forget(&interceptor->foreign_core,
        id, ops_p);
}

int kedr_coi_creation_interceptor_forget_norestore(
    struct kedr_coi_creation_interceptor* interceptor,
    void* id)
{
    return kedr_coi_foreign_core_forget_norestore(
        &interceptor->foreign_core, id);
}

int kedr_coi_bind_object(
    struct kedr_coi_creation_interceptor* interceptor,
    void* object,
    void* tie,
    size_t operation_offset,
    struct kedr_coi_creation_intermediate_info* info)
{
    //TODO: Fill handlers
    info->pre = NULL;
    info->post = NULL;

    return kedr_coi_foreign_core_bind_object(
        &interceptor->foreign_core,
        object,
        tie,
        operation_offset,
        &info->op_chained,
        &info->op_orig);
}


void kedr_coi_creation_interceptor_destroy(
    struct kedr_coi_creation_interceptor* interceptor)
{
    kedr_coi_foreign_core_finalize(&interceptor->foreign_core);
    kfree(interceptor);
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



