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

struct kedr_coi_interceptor;
struct kedr_coi_foreign_interceptor;

// Internal API of foreign interceptor(forward declaration)
static int foreign_interceptor_start(
    struct kedr_coi_foreign_interceptor* interceptor,
    struct kedr_coi_interceptor* interceptor_binded);

static void foreign_interceptor_stop(
    struct kedr_coi_foreign_interceptor* interceptor);

/* 
 * Operations which differs in interceptors of different types.
 */
struct interceptor_operations
{
    // Return instrumentor for work with given replacements
    struct kedr_coi_instrumentor_normal* (*create_instrumentor)(
        struct kedr_coi_interceptor* interceptor,
        const struct kedr_coi_replacement* replacements);
    // Free instance of the interceptor
    void (*destroy)(struct kedr_coi_interceptor* interceptor);
    // Operations for support foreign interceptors,
    // may be NULL if foreign interceptors are not supported.
    
    /*
     * Factory method.
     * 
     * Should allocate structure for foreign interceptor and fill
     * its 'ops' field.
     */
    struct kedr_coi_foreign_interceptor*
    (*alloc_foreign_interceptor_indirect)(
        struct kedr_coi_interceptor* interceptor,
        size_t operations_field_offset,
        const struct kedr_coi_foreign_intermediate* intermediate_operations);
    //
    struct kedr_coi_instrumentor_with_foreign*
    (*create_instrumentor_with_foreign)(
        struct kedr_coi_interceptor* interceptor,
        const struct kedr_coi_replacement* replacements);
};

struct kedr_coi_interceptor
{
    const char* name;
    const struct interceptor_operations* ops;
    
    int state;

    struct kedr_coi_payloads_container* payloads_container;
    struct list_head foreign_interceptors;
    
    void (*trace_unforgotten_object)(void* object);
    // Used only in the interception state
    struct kedr_coi_instrumentor_normal* instrumentor;
};


//***************Interceptor for foreign operations*******************//

struct foreign_interceptor_operations
{
    struct kedr_coi_foreign_instrumentor*
    (*create_foreign_instrumentor)(
        struct kedr_coi_foreign_interceptor* interceptor,
        struct kedr_coi_interceptor* interceptor_binded,
        struct kedr_coi_instrumentor_with_foreign* instrumentor_binded);
    
    void (*destroy)(struct kedr_coi_foreign_interceptor* interceptor);
};

struct kedr_coi_foreign_interceptor
{
    const char* name;
    const struct foreign_interceptor_operations* ops;
    
    int state;
    
    struct list_head list;
    
    void (*trace_unforgotten_object)(void* object);
    // Used only when original interceptor is in 'interception' state
    struct kedr_coi_foreign_instrumentor* instrumentor;
};


/*
 * Initialize fields of the base interceptor structure.
 */
int
interceptor_init_common(
    struct kedr_coi_interceptor* interceptor,
    const char* name,
    const struct interceptor_operations* ops,
    struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object))
{
    interceptor->name = name;
    interceptor->ops = ops;
    
    interceptor->state = interceptor_state_initialized;
    
    INIT_LIST_HEAD(&interceptor->foreign_interceptors);
    
    interceptor->trace_unforgotten_object = trace_unforgotten_object;
    
    interceptor->payloads_container = kedr_coi_payloads_container_create(
        intermediate_operations);

    return interceptor->payloads_container ? 0 : -EINVAL;
}

//*********Interceptor API implementation*****************************//

int kedr_coi_interceptor_start(struct kedr_coi_interceptor* interceptor)
{
	int result;
	
    struct kedr_coi_foreign_interceptor* foreign_interceptor;
    const struct kedr_coi_replacement* replacements;
    
	BUG_ON(interceptor->state != interceptor_state_initialized);

    result = kedr_coi_payloads_container_fix_payloads(
        interceptor->payloads_container);
    
    if(result) return result;
    
    replacements = kedr_coi_payloads_container_get_replacements(
        interceptor->payloads_container);
    
    if(list_empty(&interceptor->foreign_interceptors)
        && (interceptor->ops->create_instrumentor != NULL))
    {
        interceptor->instrumentor =
            interceptor->ops->create_instrumentor(
                interceptor,
                replacements);
        
        if(interceptor->instrumentor == NULL)
        {
            result = -EINVAL;
            goto err;
        }
    }
    else
    {
        struct kedr_coi_instrumentor_with_foreign* instrumentor_with_foreign =
            interceptor->ops->create_instrumentor_with_foreign(
                interceptor,
                replacements);
        
        if(instrumentor_with_foreign == NULL)
        {
            result = -EINVAL;
            goto err;
        }
        interceptor->instrumentor = &instrumentor_with_foreign->_instrumentor_normal;
    }
    
    interceptor->state = interceptor_state_started;

    list_for_each_entry(foreign_interceptor, &interceptor->foreign_interceptors, list)
    {
        result = foreign_interceptor_start(foreign_interceptor, interceptor);
        if(result)
        {
            goto err_foreign;
        }
    }
    
    return 0;

err_foreign:
    list_for_each_entry_continue_reverse(foreign_interceptor, &interceptor->foreign_interceptors, list)
    {
        foreign_interceptor_stop(foreign_interceptor);
    }
    
    interceptor->state = interceptor_state_initialized;

    kedr_coi_instrumentor_destroy(&interceptor->instrumentor->instrumentor_common,
        interceptor->trace_unforgotten_object);
    interceptor->instrumentor = NULL;
err:
    kedr_coi_payloads_container_release_payloads(
        interceptor->payloads_container);
    
    return result;
}


void kedr_coi_interceptor_stop(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_foreign_interceptor* foreign_interceptor;

	BUG_ON(interceptor->state == interceptor_state_uninitialized);
	
	if(interceptor->state == interceptor_state_initialized)
		return;// Assume that start() was called but failed

    list_for_each_entry(foreign_interceptor, &interceptor->foreign_interceptors, list)
    {
        foreign_interceptor_stop(foreign_interceptor);
    }

    kedr_coi_instrumentor_destroy(&interceptor->instrumentor->instrumentor_common,
        interceptor->trace_unforgotten_object);
    
    interceptor->instrumentor = NULL;
    
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
        &interceptor->instrumentor->instrumentor_common, object);
}

int kedr_coi_interceptor_forget(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);

    return kedr_coi_instrumentor_forget(
        &interceptor->instrumentor->instrumentor_common,
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
        &interceptor->instrumentor->instrumentor_common,
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
    
	BUG_ON(interceptor->state != interceptor_state_started);
    
    result = kedr_coi_instrumentor_get_orig_operation(
        interceptor->instrumentor,
        object,
        operation_offset,
        &info->op_orig);
    
    if(result < 0) return result;
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
	
	while(!list_empty(&interceptor->foreign_interceptors))
    {
        struct kedr_coi_foreign_interceptor* foreign_interceptor =
            list_first_entry(&interceptor->foreign_interceptors, struct kedr_coi_foreign_interceptor, list);
        
        list_del(&foreign_interceptor->list);
    }
    
    // For selfcontrol
	interceptor->state = interceptor_state_uninitialized;
	
    kedr_coi_payloads_container_destroy(interceptor->payloads_container,
        interceptor->name);
    
    interceptor->ops->destroy(interceptor);
}


struct kedr_coi_foreign_interceptor*
kedr_coi_foreign_interceptor_create(
    struct kedr_coi_interceptor* interceptor_indirect,
    const char* name,
    size_t foreign_operations_field_offset,
    const struct kedr_coi_foreign_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object))
{
    struct kedr_coi_foreign_interceptor* foreign_interceptor;
    
    BUG_ON(interceptor_indirect->state != interceptor_state_initialized);
    
    if(interceptor_indirect->ops->alloc_foreign_interceptor_indirect == NULL)
    {
        pr_err("This interceptor doesn't support foreign interceptors.");
        return NULL;
    }
    
    foreign_interceptor = interceptor_indirect->ops->alloc_foreign_interceptor_indirect(
        interceptor_indirect,
        foreign_operations_field_offset,
        intermediate_operations);
    
    if(foreign_interceptor == NULL) return NULL;
    // Initialize foreign interceptor structure
    foreign_interceptor->name = name;
    foreign_interceptor->trace_unforgotten_object = trace_unforgotten_object;
    
    foreign_interceptor->instrumentor = NULL;

    INIT_LIST_HEAD(&foreign_interceptor->list);
    
    foreign_interceptor->state = interceptor_state_initialized;

    list_add_tail(&foreign_interceptor->list,
        &interceptor_indirect->foreign_interceptors);
    
    return foreign_interceptor;
}


// Internal API of foreign interceptor
static int foreign_interceptor_start(
    struct kedr_coi_foreign_interceptor* interceptor,
    struct kedr_coi_interceptor* interceptor_binded)
{
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded;
    
    BUG_ON(interceptor->state != interceptor_state_initialized);
    BUG_ON(interceptor_binded->state != interceptor_state_started);
    
    instrumentor_binded = container_of(interceptor_binded->instrumentor,
        struct kedr_coi_instrumentor_with_foreign, _instrumentor_normal);
    
    interceptor->instrumentor =
        interceptor->ops->create_foreign_instrumentor(
            interceptor,
            interceptor_binded,
            instrumentor_binded);
    
    if(interceptor->instrumentor == NULL) return -EINVAL;
    
    interceptor->state = interceptor_state_started;
    
    return 0;
}

static void foreign_interceptor_stop(
    struct kedr_coi_foreign_interceptor* interceptor)
{
    if(interceptor->state == interceptor_state_initialized) return;
    
    BUG_ON(interceptor->state != interceptor_state_started);

    interceptor->state = interceptor_state_initialized;
    
    kedr_coi_instrumentor_destroy(
        &interceptor->instrumentor->instrumentor_common,
        interceptor->trace_unforgotten_object);

    interceptor->instrumentor = NULL;
}


//*********Foreign interceptor API implementation******************//
int kedr_coi_foreign_interceptor_watch(
    struct kedr_coi_foreign_interceptor* interceptor,
    void* prototype_object)
{
    if(interceptor->state == interceptor_state_initialized)
        return -EPERM;
    
    BUG_ON(interceptor->state != interceptor_state_started);
    
    return kedr_coi_instrumentor_watch(
        &interceptor->instrumentor->instrumentor_common,
        prototype_object);
}

int kedr_coi_foreign_interceptor_forget(
    struct kedr_coi_foreign_interceptor* interceptor,
    void* prototype_object)
{
    if(interceptor->state == interceptor_state_initialized)
        return -EPERM;
    
    BUG_ON(interceptor->state != interceptor_state_started);
    
    return kedr_coi_instrumentor_forget(
        &interceptor->instrumentor->instrumentor_common,
        prototype_object,
        0);
}

int kedr_coi_foreign_interceptor_forget_norestore(
    struct kedr_coi_foreign_interceptor* interceptor,
    void* prototype_object)
{
    if(interceptor->state == interceptor_state_initialized)
        return -EPERM;
    
    BUG_ON(interceptor->state != interceptor_state_started);
    
    return kedr_coi_instrumentor_forget(
        &interceptor->instrumentor->instrumentor_common,
        prototype_object,
        1);
}

int kedr_coi_bind_prototype_with_object(
    struct kedr_coi_foreign_interceptor* interceptor,
    void* prototype_object,
    void* object,
    size_t operation_offset,
    void** op_orig)
{
    BUG_ON(interceptor->state != interceptor_state_started);
    
    return kedr_coi_foreign_instrumentor_bind_prototype_with_object(
        interceptor->instrumentor,
        prototype_object,
        object,
        operation_offset,
        op_orig);
}


void kedr_coi_foreign_interceptor_destroy(
    struct kedr_coi_foreign_interceptor* interceptor)
{
    BUG_ON(interceptor->state != interceptor_state_initialized);
    interceptor->state = interceptor_state_uninitialized;

    if(!list_empty(&interceptor->list))
    {
        list_del(&interceptor->list);
    }
    
    interceptor->ops->destroy(interceptor);
}

//** Interceptor for objects with indirect operations(mostly used). **//

struct kedr_coi_interceptor_indirect
{
    struct kedr_coi_interceptor base;
    
    size_t operations_field_offset;
    size_t operations_struct_size;
};

#define interceptor_indirect(interceptor) container_of(interceptor, struct kedr_coi_interceptor_indirect, base)

struct kedr_coi_foreign_interceptor_indirect
{
    struct kedr_coi_foreign_interceptor base;
    
    size_t operations_field_offset;
    
    struct kedr_coi_replacement* replacements;
    
    //struct kedr_coi_interceptor_indirect* interceptor_binded;
};

#define foreign_interceptor_indirect(interceptor) container_of(interceptor, struct kedr_coi_foreign_interceptor_indirect, base)


static struct kedr_coi_instrumentor_with_foreign*
interceptor_indirect_op_create_instrumentor_with_foreign(
    struct kedr_coi_interceptor* interceptor,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_interceptor_indirect* interceptor_real =
        interceptor_indirect(interceptor);

    return kedr_coi_instrumentor_with_foreign_create_indirect(
            interceptor_real->operations_field_offset,
            interceptor_real->operations_struct_size,
            replacements);
}

static struct kedr_coi_instrumentor_normal*
interceptor_indirect_op_create_instrumentor(
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
interceptor_indirect_op_destroy(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_interceptor_indirect* interceptor_real =
        interceptor_indirect(interceptor);
    
    kfree(interceptor_real);
}

static struct kedr_coi_foreign_instrumentor*
foreign_interceptor_indirect_op_create_foreign_instrumentor(
    struct kedr_coi_foreign_interceptor* interceptor,
    struct kedr_coi_interceptor* interceptor_binded,
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded)
{
    struct kedr_coi_foreign_interceptor_indirect* interceptor_real;
    
    interceptor_real = foreign_interceptor_indirect(interceptor);
    
    return kedr_coi_instrumentor_with_foreign_create_foreign_indirect(
        instrumentor_binded,
        interceptor_real->operations_field_offset,
        interceptor_real->replacements);
}

static void
foreign_interceptor_indirect_op_destroy(
    struct kedr_coi_foreign_interceptor* interceptor)
{
    struct kedr_coi_foreign_interceptor_indirect* interceptor_real =
        container_of(interceptor, typeof(*interceptor_real), base);
    
    kfree(interceptor_real->replacements);
    
    kfree(interceptor_real);
}

struct foreign_interceptor_operations foreign_interceptor_indirect_ops =
{
    .create_foreign_instrumentor = foreign_interceptor_indirect_op_create_foreign_instrumentor,
    .destroy = foreign_interceptor_indirect_op_destroy
};

static struct kedr_coi_foreign_interceptor*
interceptor_indirect_op_alloc_foreign_interceptor_indirect(
    struct kedr_coi_interceptor* interceptor,
    size_t operations_field_offset,
    const struct kedr_coi_foreign_intermediate* intermediate_operations)
{
    int n_intermediates;
    int i;
    const struct kedr_coi_foreign_intermediate* intermediate_operation;
    struct kedr_coi_replacement* replacements;
    struct kedr_coi_foreign_interceptor_indirect* foreign_interceptor;
    
    if((intermediate_operations == NULL)
        || (intermediate_operations[0].operation_offset == -1))
    {
        pr_err("Foreign instrumentor cannot be created without intermediate operations.");
        return NULL;
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
        pr_err("Failed to allocate replacements for foreign interceptor.");
        return NULL;
    }
    
    i = 0;
    for(intermediate_operation = intermediate_operations;
        intermediate_operation->operation_offset != -1;
        intermediate_operation++)
    {
        replacements[i].operation_offset = intermediate_operation->operation_offset;
        replacements[i].repl = intermediate_operation->repl;
        
        i++;
    }
    BUG_ON(i != n_intermediates);
    replacements[n_intermediates].operation_offset = -1;

    
    foreign_interceptor = kmalloc(sizeof(*foreign_interceptor), GFP_KERNEL);
    
    if(foreign_interceptor == NULL)
    {
        pr_err("Failed to allocate foreign interceptor structure.");
        kfree(replacements);
        return NULL;
    }
    
    foreign_interceptor->operations_field_offset = operations_field_offset;
    foreign_interceptor->replacements = replacements;
    foreign_interceptor->base.ops = &foreign_interceptor_indirect_ops;
    
    return &foreign_interceptor->base;
}

static struct interceptor_operations interceptor_indirect_ops =
{
    .create_instrumentor = interceptor_indirect_op_create_instrumentor,
    .destroy = interceptor_indirect_op_destroy,
    
    .alloc_foreign_interceptor_indirect = interceptor_indirect_op_alloc_foreign_interceptor_indirect,
    .create_instrumentor_with_foreign = interceptor_indirect_op_create_instrumentor_with_foreign
};

struct kedr_coi_interceptor*
kedr_coi_interceptor_create(const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object))
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
        intermediate_operations,
        trace_unforgotten_object);
    
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


static struct kedr_coi_instrumentor_normal*
interceptor_direct_op_create_instrumentor(
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
        intermediate_operations,
        trace_unforgotten_object);
    
    if(result)
    {
        pr_err("Failed to initialize interceptor.");
        kfree(interceptor);
        
        return NULL;
    }

    interceptor->object_struct_size = object_struct_size;
    
    return &interceptor->base;
}
