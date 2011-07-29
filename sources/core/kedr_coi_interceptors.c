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

#include "kedr_coi_payloads_internal.h"
#include "kedr_coi_payloads_internal.h"

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

/* 
 * Operations which differs in interceptors of different types.
 */
struct interceptor_operations
{
    // Return instrumentor for work with given replacements
    struct kedr_coi_instrumentor* (*get_instrumentor)(
        struct kedr_coi_interceptor* interceptor,
        const struct kedr_coi_replacement* replacements);
    // Free instance of the interceptor
    void (*free_instance)(struct kedr_coi_interceptor* interceptor);
};


struct kedr_coi_interceptor
{
    const char* name;
    const struct interceptor_operations* ops;
    
    int state;

    struct kedr_coi_payloads_container* payloads_container;
    // Is used only in the interception state
    struct kedr_coi_instrumentor* instrumentor;
};


/*
 * Initialize fields of the base interceptor structure.
 */
int
interceptor_init_common(
    struct kedr_coi_interceptor* interceptor,
    const char* name,
    const struct interceptor_operations* ops,
    struct kedr_coi_intermediate* intermediate_operations)
{
    interceptor->name = name;
    interceptor->ops = ops;
    
    interceptor->state = interceptor_state_initialized;
    
    interceptor->payloads_container = kedr_coi_payloads_container_create(
        intermediate_operations);

    return interceptor->payloads_container ? 0 : -EINVAL;
}

int kedr_coi_interceptor_start(struct kedr_coi_interceptor* interceptor)
{
	int result;
	
    struct kedr_coi_instrumentor* instrumentor;
    
	BUG_ON(interceptor->state != interceptor_state_initialized);

    result = kedr_coi_payloads_container_fix_payloads(
        interceptor->payloads_container);
    
    if(result) return result;
    
    instrumentor = interceptor->ops->get_instrumentor(interceptor,
        kedr_coi_payloads_container_get_replacements(interceptor->payloads_container));

    if(instrumentor == NULL)
    {
        kedr_coi_payloads_container_release_payloads(
            interceptor->payloads_container);
        
        return -EINVAL;//some error
    }

    interceptor->instrumentor = instrumentor;
    
    interceptor->state = interceptor_state_started;

    return 0;
}


void kedr_coi_interceptor_stop(struct kedr_coi_interceptor* interceptor,
	void (*trace_unforgotten_object)(void* object))
{
	BUG_ON(interceptor->state == interceptor_state_uninitialized);
	
	if(interceptor->state == interceptor_state_initialized)
		return;// Assume that start() was called but failed

    kedr_coi_instrumentor_destroy(interceptor->instrumentor,
        trace_unforgotten_object);
    
    kedr_coi_payloads_container_release_payloads(
         interceptor->payloads_container);
	
	interceptor->state = interceptor_state_initialized;
}

int kedr_coi_interceptor_watch(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);
	
    return kedr_coi_instrumentor_watch(
        interceptor->instrumentor, object);
}

int kedr_coi_interceptor_forget(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);

    return kedr_coi_instrumentor_forget(
        interceptor->instrumentor,
        object,
        0);
}

int kedr_coi_interceptor_forget_norestore(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);

    return kedr_coi_instrumentor_forget(
        interceptor->instrumentor,
        object,
        1);
}

int kedr_coi_payload_register(
	struct kedr_coi_interceptor* interceptor,
	struct kedr_coi_payload* payload)
{
	// "hot" registering of payload is permitted at abstract level
	BUG_ON((interceptor->state != interceptor_state_initialized)
		&& (interceptor->state != interceptor_state_started));

    return kedr_coi_payloads_container_register_payload(
        interceptor->payloads_container, payload);

}

void kedr_coi_payload_unregister(
	struct kedr_coi_interceptor* interceptor,
	struct kedr_coi_payload* payload)
{
	BUG_ON((interceptor->state != interceptor_state_initialized)
		&& (interceptor->state != interceptor_state_started));

    kedr_coi_payloads_container_unregister_payload(
        interceptor->payloads_container, payload);
}


int kedr_coi_interceptor_get_intermediate_info(
	struct kedr_coi_interceptor* interceptor,
	const void* object,
	size_t operation_offset,
	struct kedr_coi_intermediate_info* info)
{
    int result;
    const void* ops_orig;
    
	BUG_ON(interceptor->state != interceptor_state_started);
    
    result = kedr_coi_instrumentor_get_orig_operations(
        interceptor->instrumentor,
        object,
        &ops_orig);
    
    if(result < 0) return result;
    info->op_orig = *(void**)((const char*)ops_orig + operation_offset);
    if(result == 0)
    {
        result = kedr_coi_payloads_container_get_handlers(
            interceptor->payloads_container,
            operation_offset,
            &info->pre,
            &info->post);
        
        if(result)
        {
            // as if no handlers
            info->pre = NULL;
            info->post = NULL;
        }
    }
    else
    {
        // without handlers - as if no interception at all.
        info->pre = NULL;
        info->post = NULL;
    }
    
    return 0;
}


void kedr_coi_interceptor_destroy(
	struct kedr_coi_interceptor* interceptor)
{
	BUG_ON(interceptor->state != interceptor_state_initialized);
	
	// For selfcontrol
	interceptor->state = interceptor_state_uninitialized;
	
    kedr_coi_payloads_container_destroy(interceptor->payloads_container,
        interceptor->name);
    
    interceptor->ops->free_instance(interceptor);
}

/* Interceptor for objects with indirect operations(mostly used). */

struct kedr_coi_interceptor_indirect
{
    struct kedr_coi_interceptor base;
    
    size_t operations_field_offset;
    size_t operations_struct_size;
};

#define interceptor_indirect(interceptor) container_of(interceptor, struct kedr_coi_interceptor_indirect, base)

static struct kedr_coi_instrumentor*
interceptor_indirect_op_get_instrumentor(
    struct kedr_coi_interceptor* interceptor,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_interceptor_indirect* interceptor_real =
        interceptor_indirect(interceptor);
    
    return kedr_coi_instrumentor_create_indirect(
        interceptor_real->operations_field_offset,
        interceptor_real->operations_struct_size,
        replacements);
}


static void
interceptor_indirect_op_free_instance(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_interceptor_indirect* interceptor_real =
        interceptor_indirect(interceptor);
    
    kfree(interceptor_real);
}


static struct interceptor_operations interceptor_indirect_ops =
{
    .get_instrumentor = interceptor_indirect_op_get_instrumentor,
    .free_instance = interceptor_indirect_op_free_instance
};

struct kedr_coi_interceptor*
kedr_coi_interceptor_create(const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_intermediate* intermediate_operations)
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
        intermediate_operations);
    
    if(result)
    {
        pr_err("Failed to initialize interceptor.");
        kfree(interceptor);
        
        return NULL;
    }

    interceptor->operations_field_offset = operations_field_offset;
    interceptor->operations_struct_size = operations_struct_size;
    
    return &interceptor->base;
}


/* Interceptor for objects with direct operations. */

struct kedr_coi_interceptor_direct
{
    struct kedr_coi_interceptor base;
    
    size_t object_struct_size;
};

#define interceptor_direct(interceptor) container_of(interceptor, struct kedr_coi_interceptor_direct, base)


static struct kedr_coi_instrumentor*
interceptor_direct_op_get_instrumentor(
    struct kedr_coi_interceptor* interceptor,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_interceptor_direct* interceptor_real =
        interceptor_direct(interceptor);
    
    return kedr_coi_instrumentor_create_direct(
        interceptor_real->object_struct_size,
        replacements);
}


static void
interceptor_direct_op_free_instance(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_interceptor_direct* interceptor_real =
        interceptor_direct(interceptor);
    
    kfree(interceptor_real);
}


static struct interceptor_operations interceptor_direct_ops =
{
    .get_instrumentor = interceptor_direct_op_get_instrumentor,
    .free_instance = interceptor_direct_op_free_instance
};

struct kedr_coi_interceptor*
kedr_coi_interceptor_create_direct(const char* name,
    size_t object_struct_size,
    struct kedr_coi_intermediate* intermediate_operations)
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
        intermediate_operations);
    
    if(result)
    {
        pr_err("Failed to initialize interceptor.");
        kfree(interceptor);
        
        return NULL;
    }

    interceptor->object_struct_size = object_struct_size;
    
    return &interceptor->base;
}


/* Interceptor for objects with foreign operations */
struct kedr_coi_foreign_interceptor
{
    const char* name;
    
    int state;
    
    struct kedr_coi_foreign_payloads_container* payloads_container;
    struct kedr_coi_instrumentor* instrumentor;
    
    size_t operations_field_offset;
    size_t operations_struct_size;
    
    size_t foreign_operations_field_offset;
};


struct kedr_coi_foreign_interceptor*
kedr_coi_foreign_interceptor_create(
    const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    size_t foreign_operations_field_offset,
    struct kedr_coi_foreign_intermediate* intermediate_operations)
{
    struct kedr_coi_foreign_interceptor* interceptor;
    
    interceptor = kmalloc(sizeof(*interceptor), GFP_KERNEL);
    
    if(interceptor == NULL)
    {
        pr_err("Cannot allocate operations interceptor.");
        
        return NULL;
    }
    
    interceptor->payloads_container =
        kedr_coi_foreign_payloads_container_create(
            intermediate_operations);
    
    if(interceptor->payloads_container == NULL)
    {
        kfree(interceptor);
        
        return NULL;
    }

    interceptor->name = name;
    interceptor->state = interceptor_state_initialized;
    
    interceptor->operations_field_offset = operations_field_offset;
    interceptor->operations_struct_size = operations_struct_size;
    interceptor->foreign_operations_field_offset = foreign_operations_field_offset;

    return interceptor;
}



int
kedr_coi_foreign_interceptor_start(
    struct kedr_coi_foreign_interceptor* interceptor)
{
    int result;
    struct kedr_coi_instrumentor* instrumentor;

	BUG_ON(interceptor->state != interceptor_state_initialized);
    
    result = kedr_coi_foreign_payloads_container_fix_payloads(
        interceptor->payloads_container);
    
    if(result) return result;
    
    instrumentor = kedr_coi_instrumentor_create_indirect(
        interceptor->operations_field_offset,
        interceptor->operations_struct_size,
        kedr_coi_foreign_payloads_container_get_replacements(interceptor->payloads_container));

    if(instrumentor == NULL)
    {
        kedr_coi_foreign_payloads_container_release_payloads(
            interceptor->payloads_container);
        
        return -EINVAL;//some error
    }

    interceptor->instrumentor = instrumentor;
    
    interceptor->state = interceptor_state_started;
    
    return 0;
}


void
kedr_coi_foreign_interceptor_stop(
    struct kedr_coi_foreign_interceptor* interceptor,
    void (*trace_unforgotten_object)(void* object))
{
	BUG_ON(interceptor->state == interceptor_state_uninitialized);
	
	if(interceptor->state == interceptor_state_initialized)
		return;// Assume that start() was called but failed

    kedr_coi_instrumentor_destroy(interceptor->instrumentor,
        trace_unforgotten_object);
    
    kedr_coi_foreign_payloads_container_release_payloads(
        interceptor->payloads_container);
    
    interceptor->state = interceptor_state_initialized;
}

int
kedr_coi_foreign_interceptor_watch(
    struct kedr_coi_foreign_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);
    
    return kedr_coi_instrumentor_watch(
        interceptor->instrumentor,
        object);
}

int
kedr_coi_foreign_interceptor_forget(
    struct kedr_coi_foreign_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);
    
    return kedr_coi_instrumentor_forget(
        interceptor->instrumentor,
        object,
        0);
}


int
kedr_coi_foreign_interceptor_forget_norestore(
    struct kedr_coi_foreign_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);
    
    return kedr_coi_instrumentor_forget(
        interceptor->instrumentor,
        object,
        1);
}

int
kedr_coi_foreign_interceptor_restore_copy(
    struct kedr_coi_foreign_interceptor* interceptor,
    void* object,
    void* foreign_object,
    struct kedr_coi_foreign_intermediate_info* info)
{
    const void** ops_foreign;
    const void* ops_orig;
    int result;

	BUG_ON(interceptor->state != interceptor_state_started);
    
    result = kedr_coi_instrumentor_get_orig_operations(
        interceptor->instrumentor,
        object,
        &ops_orig);
    
    if(result < 0) return result;
    
    ops_foreign = (const void**)((char*)foreign_object
        + interceptor->foreign_operations_field_offset);
    
    *ops_foreign = ops_orig;
    
    if(result == 0)
    {
        result = kedr_coi_foreign_payloads_container_get_handlers(
            interceptor->payloads_container,
            &info->on_create_handlers);
        
        if(result)
        {
            //as if no handlers
            info->on_create_handlers = NULL;
        }
    }
    else
    {
        // as if no interception(no handlers)
        info->on_create_handlers = NULL;
    }
    return 0;
}

int kedr_coi_foreign_payload_register(
    struct kedr_coi_foreign_interceptor* interceptor,
    struct kedr_coi_foreign_payload* payload)
{
	// "hot" registering of payload is permitted at abstract level
	BUG_ON((interceptor->state != interceptor_state_initialized)
		&& (interceptor->state != interceptor_state_started));
    
    return kedr_coi_foreign_payloads_container_register_payload(
        interceptor->payloads_container, payload);
}

void
kedr_coi_foreign_payload_unregister(
    struct kedr_coi_foreign_interceptor* interceptor,
    struct kedr_coi_foreign_payload* payload)
{
	BUG_ON((interceptor->state != interceptor_state_initialized)
		&& (interceptor->state != interceptor_state_started));
    
    kedr_coi_foreign_payloads_container_unregister_payload(
        interceptor->payloads_container, payload);
}

void
kedr_coi_foreign_interceptor_destroy(
    struct kedr_coi_foreign_interceptor* interceptor)
{
    kedr_coi_foreign_payloads_container_destroy(
        interceptor->payloads_container,
        interceptor->name);
    
    kfree(interceptor);
}