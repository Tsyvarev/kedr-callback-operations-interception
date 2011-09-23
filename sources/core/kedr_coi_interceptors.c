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
 * Element of the payload registration.
 */
struct payload_elem
{
    // Payload itself
    struct kedr_coi_payload* payload;
    // Element in the List of registered payload elements.
    struct list_head list;
    // Whether this payload element is fixed
    int is_fixed;
    /*
     *  Element in the list of used payload elements.
     * 
     * NOTE: This list is constant after fix payloads.
     */
    struct list_head list_used;
};

// Initialize payload element structure
static void payload_elem_init(
    struct payload_elem* elem,
    struct kedr_coi_payload* payload)
{
    elem->payload = payload;
    INIT_LIST_HEAD(&elem->list);
    elem->is_fixed = 0;
    INIT_LIST_HEAD(&elem->list_used);
}

/*
 *  Information about operation, which may be intercepted.
 */
struct operation_info
{
    // Element in the list of all operations which may be intercepted
    struct list_head list;
    
    size_t operation_offset;
    
    void* repl;
    int group_id;

    // Whether interception of this operation is used
    int is_intercepted; 

    // Next fields has a sence only when 'is_intercepted' is not 0
    // NULL-terminated array of per-handlers or NULL
    void** pre;
    // NULL-terminated array of post-handlers or NULL
    void** post;
    // Counters of pre- and post- handlers.
    int n_pre;
    int n_post;
};

// Initialize operation information structure
void operation_info_init(struct operation_info* operation,
    const struct kedr_coi_intermediate* intermediate)
{
    operation->operation_offset = intermediate->operation_offset;
    operation->repl = intermediate->repl;
    operation->group_id = intermediate->group_id;
    
    INIT_LIST_HEAD(&operation->list);
    
    operation->is_intercepted = 0;
}

/*
 *  Mark operation as intercepted.
 * Do nothing if it is already intercepted.
 */
static void
operation_info_intercept(struct operation_info* operation)
{
    if(operation->is_intercepted) return;//already intercepted
    
    operation->pre = NULL;
    operation->post = NULL;

    operation->n_pre = 0;
    operation->n_post = 0;

    operation->is_intercepted = 1;
}

/* 
 * Mark operation as not intercepted.
 * Also release any resources concerned interception.
 * 
 * Do nothing if operation is not intercepted.
 */
static void
operation_info_clear_interception(struct operation_info* operation)
{
    if(!operation->is_intercepted) return;

    operation->is_intercepted = 0;

    kfree(operation->pre);
    kfree(operation->post);
}

/*
 * Add pre-handler to the array of pre-handlers of operation.
 * 
 * Return 0 on success and negative error code otherwise.
 */
static int
operation_info_add_pre(struct operation_info* operation,
    void* pre)
{
    int n_pre_new = operation->n_pre + 1;
    void** pre_new = krealloc(operation->pre,
        sizeof(*operation->pre) * (n_pre_new + 1), GFP_KERNEL);
    if(pre_new == NULL)
    {
        pr_err("Failed to allocate array of pre handlers for operation.");
        return -ENOMEM;
    }
    
    pre_new[n_pre_new - 1] = pre;
    pre_new[n_pre_new] = NULL;
    
    operation->pre = pre_new;
    operation->n_pre = n_pre_new;
    
    
    return 0;

}

/*
 * Add post-handler to the array of post-handlers of operation.
 * 
 * Return 0 on success and negative error code otherwise.
 */
static int
operation_info_add_post(struct operation_info* operation,
    void* post)
{
    int n_post_new = operation->n_post + 1;
    void** post_new = krealloc(operation->post,
        sizeof(*operation->post) * (n_post_new + 1),
        GFP_KERNEL);
    if(post_new == NULL)
    {
        pr_err("Failed to allocate array of post handlers for operation.");
        return -ENOMEM;
    }
    
    post_new[n_post_new - 1] = post;
    post_new[n_post_new] = NULL;
    
    operation->post = post_new;
    operation->n_post = n_post_new;
    
    return 0;
}

// Forward declarations
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
    /*
     * Return instrumentor for work with given replacements.
     */
    struct kedr_coi_instrumentor_normal* (*create_instrumentor)(
        struct kedr_coi_interceptor* interceptor,
        const struct kedr_coi_replacement* replacements);
    // Free instance of the interceptor
    void (*destroy)(struct kedr_coi_interceptor* interceptor);
    
    /*
     * Factory method.
     * 
     * Should allocate structure for foreign interceptor and set
     * its 'ops' field.
     * 
     * May be NULL if foreign interceptors is not supported for
     * current one.
     */
    struct kedr_coi_foreign_interceptor*
    (*alloc_foreign_interceptor_indirect)(
        struct kedr_coi_interceptor* interceptor,
        size_t operations_field_offset);
    
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
        const struct kedr_coi_replacement* replacements);
};

/*
 * Basic instrumentor structure.
 */
struct kedr_coi_interceptor
{
    const char* name;
    const struct interceptor_operations* ops;
    void (*trace_unforgotten_object)(void* object);
    // Current state of the interceptor
    int state;
    // List of the registered payload elements.
    struct list_head payload_elems;
    // List of foreign interceptors created by this one
    struct list_head foreign_interceptors;
    /*
     *  Protect list of registered payload elements from concurrent
     * access.
     *  Protect list of foreign interceptors from concurrent access.
     */
    struct mutex m;
    
    // List of all operations available for interception
    struct list_head operations;
    // Next fields are used only when start to intercept.
    
    // List of used payload elements
    struct list_head payload_elems_used;
    // Replacements collected from all used payloads
    struct kedr_coi_replacement* replacements;
    // Instrumentor used for instrument objects' operations.
    struct kedr_coi_instrumentor_normal* instrumentor;
};

/*
 * Return information about operation with given offset.
 * 
 * If not found, return NULL.
 */
static struct operation_info*
interceptor_find_operation(
    struct kedr_coi_interceptor* interceptor,
    size_t operation_offset)
{
    struct operation_info* operation;
    list_for_each_entry(operation, &interceptor->operations, list)
    {
        if(operation->operation_offset == operation_offset)
            return operation;
    }
    
    return NULL;
}

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
    int result;
    
    interceptor->name = name;
    interceptor->ops = ops;
    
    interceptor->state = interceptor_state_initialized;
    
    interceptor->trace_unforgotten_object = trace_unforgotten_object;
    
    mutex_init(&interceptor->m);
    INIT_LIST_HEAD(&interceptor->payload_elems);
    INIT_LIST_HEAD(&interceptor->foreign_interceptors);
    
    INIT_LIST_HEAD(&interceptor->operations);
    // Fill operations list from intermediate operations array
    if(intermediate_operations)
    {
        const struct kedr_coi_intermediate* intermediate;
        
        for(intermediate = intermediate_operations;
            intermediate->operation_offset != -1;
            intermediate++)
        {
            struct operation_info* operation =
                kmalloc(sizeof(*operation), GFP_KERNEL);
            if(operation == NULL)
            {
                pr_err("Failed to allocate information about operation.");
                result = -ENOMEM;
                goto err_operation;
            }
            operation_info_init(operation, intermediate);
            list_add_tail(&operation->list, &interceptor->operations);
        }
    }

    interceptor->replacements = NULL;
    
    return 0;

err_operation:
    while(!list_empty(&interceptor->operations))
    {
        struct operation_info* operation =
            list_first_entry(&interceptor->operations, struct operation_info, list);
        list_del(&operation->list);
        kfree(operation);
    }
    return result;
}

/*
 * Actions which should be performed before fixing payload element.
 * 
 * If return not 0, payload element shouldn't be fixed.
 */
static int interceptor_fix_payload_elem(
    struct kedr_coi_interceptor* interceptor,
    struct payload_elem* elem)
{
    if(elem->payload->mod != NULL)
        if(try_module_get(elem->payload->mod) == 0)
            return -EBUSY;
    
    list_add_tail(&elem->list_used, &interceptor->payload_elems_used);

    return 0;
}

/*
 * Actions which should be performed after releasing payload element.
 */
static void interceptor_release_payload_elem(
    struct kedr_coi_interceptor* interceptor,
    struct payload_elem* elem)
{
    if(elem->payload->mod != NULL)
        module_put(elem->payload->mod);
    
    list_del(&elem->list_used);
}


/*
 * Fix all payload elements.
 * 
 * Return 0 on success and negative error code otherwise.
 */
static int interceptor_fix_payloads(
    struct kedr_coi_interceptor* interceptor)
{
    struct payload_elem* elem;
    list_for_each_entry(elem, &interceptor->payload_elems, list)
    {
        if(!interceptor_fix_payload_elem(interceptor, elem))
        {
            elem->is_fixed = 1;
        }
        // error in fixing one payload is non-fatal
    }
    return 0;
}

/*
 * Release all payloads which was fixed.
 */
static void interceptor_release_payloads(
    struct kedr_coi_interceptor* interceptor)
{
    struct payload_elem* elem;
    list_for_each_entry(elem, &interceptor->payload_elems, list)
    {
        if(!elem->is_fixed) continue;

        interceptor_release_payload_elem(interceptor, elem);
        elem->is_fixed = 0;
    }
}

/*
 * Check payload before registration.
 * 
 * Return 0 if payload allowed to register and negative error code
 * otherwise.
 */
static int interceptor_check_payload(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload* payload)
{
    // Verify that payload requires to intercept only known operations
    if(payload->pre_handlers)
    {
        struct kedr_coi_pre_handler* pre_handler;
        for(pre_handler = payload->pre_handlers;
            pre_handler->operation_offset != -1;
            pre_handler++)
        {
            if(interceptor_find_operation(interceptor,
                pre_handler->operation_offset) == NULL)
            {
                pr_err("Cannot register payload %p because it requires "
                    "to intercept unknown operation with offset %zu.",
                        payload, pre_handler->operation_offset);
                return -EINVAL;
            }
        }
    }
    
    if(payload->post_handlers)
    {
        struct kedr_coi_post_handler* post_handler;
        for(post_handler = payload->post_handlers;
            post_handler->operation_offset != -1;
            post_handler++)
        {
            if(interceptor_find_operation(interceptor,
                post_handler->operation_offset) == NULL)
            {
                pr_err("Cannot register payload %p because it requires "
                    "to intercept unknown operation with offset %zu.",
                        payload, post_handler->operation_offset);
                return -EINVAL;
            }
        }
    }

    return 0;
}

/*
 * Mark operation as intercepted.
 * 
 * Also process operations grouping.
 * 
 * NOTE: Function's effect is not solely revertable!
 */
static void
intercept_operation(
    struct kedr_coi_interceptor* interceptor,
    struct operation_info* operation)
{
    if(operation->is_intercepted) return;
    
    operation_info_intercept(operation);
    
    if(operation->group_id != 0)
    {
        int group_id = operation->group_id;
        struct operation_info* operation_tmp;
        list_for_each_entry(operation_tmp, &interceptor->operations, list)
        {
            if(operation_tmp->group_id != group_id) continue;
            if(operation_tmp->is_intercepted) continue;
            operation_info_intercept(operation_tmp);
        }
    }
}

/*
 * Use payload for interception.
 * 
 * May be called only for payload which is fixed.
 * 
 * Return 0 on success and negative error code on fail.
 * 
 * NOTE: On error does not rollback changes.
 */
static int interceptor_use_payload_elem(
    struct kedr_coi_interceptor* interceptor,
    struct payload_elem* elem)
{
    struct kedr_coi_payload* payload = elem->payload;
    
    BUG_ON(!elem->is_fixed);
    
    if(payload->pre_handlers)
    {
        struct kedr_coi_pre_handler* pre_handler;
        for(pre_handler = payload->pre_handlers;
            pre_handler->operation_offset != -1;
            pre_handler++)
        {
            int result;
            struct operation_info* operation = interceptor_find_operation(
                interceptor, pre_handler->operation_offset);

            BUG_ON(operation == NULL);//payloads should be checked when registered
            intercept_operation(interceptor, operation);
            
            result = operation_info_add_pre(operation, pre_handler->func);
            if(result) return result;
        }
    }
    
    if(payload->post_handlers)
    {
        struct kedr_coi_post_handler* post_handler;
        for(post_handler = payload->post_handlers;
            post_handler->operation_offset != -1;
            post_handler++)
        {
            int result;
            struct operation_info* operation = interceptor_find_operation(
                interceptor, post_handler->operation_offset);

            BUG_ON(operation == NULL);//payloads should be checked when registered
            intercept_operation(interceptor, operation);

            result = operation_info_add_post(operation, post_handler->func);
            if(result) return result;
        }
    }

    return 0;
}

/*
 * Cancel using of all payloads in interception.
 */
static void interceptor_unuse_payloads(
    struct kedr_coi_interceptor* interceptor)
{
    struct operation_info* operation;
    list_for_each_entry(operation, &interceptor->operations, list)
    {
        operation_info_clear_interception(operation);
    }
}

/*
 * Create replacements array according to fixed payloads content.
 */
static int
interceptor_create_replacements(
    struct kedr_coi_interceptor* interceptor)
{
    struct operation_info* operation;
    int replacements_n = 0;
    int i;
    
    // Count
    list_for_each_entry(operation, &interceptor->operations, list)
    {
        if(operation->is_intercepted)
            replacements_n++;
    }
    
    if(replacements_n == 0)
    {
        interceptor->replacements = NULL;//nothing to replace
        return 0;
    }
    // Allocate array
    interceptor->replacements =
        kmalloc(sizeof(*interceptor->replacements) * (replacements_n + 1),
        GFP_KERNEL);
    
    if(interceptor->replacements == NULL)
    {
        pr_err("Failed to allocate replacements array");
        return -ENOMEM;
    }
    
    // Filling
    i = 0;
    list_for_each_entry(operation, &interceptor->operations, list)
    {
        if(operation->is_intercepted)
        {
            struct kedr_coi_replacement* replacement =
                &interceptor->replacements[i];
        
            replacement->operation_offset = operation->operation_offset;
            replacement->repl = operation->repl;
            
            i++;
        }
    }
    BUG_ON(i != replacements_n);
    
    interceptor->replacements[replacements_n].operation_offset = -1;
    
    return 0;
}

//***************Interceptor for foreign operations*******************//

/*
 * Operations of foreign interceptor which may be different for
 * interceptors of different type.
 */
struct foreign_interceptor_operations
{
    /*
     * Should create instrumentor for foreign interceptor.
     * 
     * 'interceptor_binded' - normal interceptor, for which this
     * interceptor is created.
     * 'instrumentor_binded' - instrumentor with foreign support which
     * is created for binded interceptor.
     */
    struct kedr_coi_foreign_instrumentor*
    (*create_foreign_instrumentor)(
        struct kedr_coi_foreign_interceptor* interceptor,
        const struct kedr_coi_replacement* replacements,
        struct kedr_coi_interceptor* interceptor_binded,
        struct kedr_coi_instrumentor_with_foreign* instrumentor_binded);
    
    void (*destroy)(struct kedr_coi_foreign_interceptor* interceptor);
};

struct kedr_coi_foreign_interceptor
{
    const char* name;
    const struct foreign_interceptor_operations* ops;
    // Current state of the interceptor
    int state;
    // Element of the list of foreign interceptor binded with normal one.
    struct list_head list;
    
    void (*trace_unforgotten_object)(void* object);
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

// Initialize foreign interceptor structure.
static int foreign_interceptor_init(
    struct kedr_coi_foreign_interceptor* interceptor,
    const char* name,
    const struct kedr_coi_foreign_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object))
{
    struct kedr_coi_replacement* replacements;
    const struct kedr_coi_foreign_intermediate* intermediate_operation;
    int n_intermediates;
    int i;

    if((intermediate_operations == NULL)
        || (intermediate_operations[0].operation_offset == -1))
    {
        pr_err("Foreign instrumentor cannot be created without intermediate operations.");
        return -EINVAL;
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
        return -ENOMEM;
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

    interceptor->replacements = replacements;
    
    interceptor->name = name;
    interceptor->trace_unforgotten_object = trace_unforgotten_object;
    
    interceptor->instrumentor = NULL;

    INIT_LIST_HEAD(&interceptor->list);
    
    interceptor->state = interceptor_state_initialized;
    
    return 0;
}

//*********Interceptor API implementation*****************************//

int kedr_coi_interceptor_start(struct kedr_coi_interceptor* interceptor)
{
	int result;
	
    struct kedr_coi_foreign_interceptor* foreign_interceptor;
    struct payload_elem* elem;
    
	BUG_ON(interceptor->state != interceptor_state_initialized);

    INIT_LIST_HEAD(&interceptor->payload_elems_used);
    // Fix payloads for allow its using in interception.
    result = interceptor_fix_payloads(interceptor);
    
    if(result) return result;
    // Use all fixed payloads for interception.
    list_for_each_entry(elem, &interceptor->payload_elems_used, list_used)
    {
        result = interceptor_use_payload_elem(interceptor, elem);
        
        if(result) goto err_use_payloads;
    }
    
    result = interceptor_create_replacements(interceptor);
    
    if(result) goto err_create_replacements;
    
    if(list_empty(&interceptor->foreign_interceptors)
        && (interceptor->ops->create_instrumentor != NULL))
    {
        /*
         *  Create normal instrumentor, because no foreign interceptors
         * was created for this one.
         */
        interceptor->instrumentor =
            interceptor->ops->create_instrumentor(
                interceptor,
                interceptor->replacements);
        
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
                interceptor->replacements);
        
        if(instrumentor_with_foreign == NULL)
        {
            result = -EINVAL;
            goto err_create_instrumentor;
        }
        interceptor->instrumentor = &instrumentor_with_foreign->_instrumentor_normal;
    }
    
    interceptor->state = interceptor_state_started;
    // Also start all foreign interceptors created for this one.
    list_for_each_entry(foreign_interceptor, &interceptor->foreign_interceptors, list)
    {
        result = foreign_interceptor_start(foreign_interceptor, interceptor);
        if(result)
        {
            goto err_foreign_start;
        }
    }
    
    return 0;

err_foreign_start:
    list_for_each_entry_continue_reverse(foreign_interceptor,
        &interceptor->foreign_interceptors, list)
    {
        foreign_interceptor_stop(foreign_interceptor);
    }
    
    interceptor->state = interceptor_state_initialized;

    kedr_coi_instrumentor_destroy(&interceptor->instrumentor->instrumentor_common,
        interceptor->trace_unforgotten_object);

    interceptor->instrumentor = NULL;

err_create_instrumentor:
    kfree(interceptor->replacements);
err_create_replacements:
    
err_use_payloads:
    interceptor_unuse_payloads(interceptor);
    
    interceptor_release_payloads(interceptor);

    return result;
}


void kedr_coi_interceptor_stop(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_foreign_interceptor* foreign_interceptor;

	BUG_ON(interceptor->state == interceptor_state_uninitialized);
	
	if(interceptor->state == interceptor_state_initialized)
		return;// Assume that start() was called but failed

    // First - stop all foreign interceptors created for this one
    list_for_each_entry(foreign_interceptor, &interceptor->foreign_interceptors, list)
    {
        foreign_interceptor_stop(foreign_interceptor);
    }

    interceptor->state = interceptor_state_initialized;

    kedr_coi_instrumentor_destroy(&interceptor->instrumentor->instrumentor_common,
        interceptor->trace_unforgotten_object);
    
    interceptor->instrumentor = NULL;
    
    interceptor_unuse_payloads(interceptor);
    
    interceptor_release_payloads(interceptor);
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
    int result;
    struct payload_elem* elem;
    
    BUG_ON((interceptor->state != interceptor_state_initialized)
        && (interceptor->state != interceptor_state_started));
    
    if(interceptor->state == interceptor_state_started)
    {
        pr_err("Cannot register payload because interceptor is "
            "started. Please, stop it.");
        return -EBUSY;
    }

    result = interceptor_check_payload(interceptor, payload);
    if(result) return result;

    elem = kmalloc(sizeof(*elem), GFP_KERNEL);
    if(elem == NULL)
    {
        pr_err("Failed to allocate registered payload element.");
        return -ENOMEM;
    }

    payload_elem_init(elem, payload);
    
    result = mutex_lock_killable(&interceptor->m);
    if(result)
    {
        kfree(elem);
        return result;
    }

    // Check that item wasn't added before.
    {
        struct payload_elem* elem_tmp;
        list_for_each_entry(elem_tmp, &interceptor->payload_elems, list)
        {
            if(elem_tmp->payload == payload)
            {
                pr_err("Payload %p already registered. It is an error "
                    "to register payload twice.", payload);
                result = -EEXIST;
                goto err;
            }
        }
    }
    
    list_add_tail(&elem->list, &interceptor->payload_elems);

    mutex_unlock(&interceptor->m);
    
    return 0;

err:
    mutex_unlock(&interceptor->m);
    kfree(elem);
    return result;
}

int kedr_coi_payload_unregister(
	struct kedr_coi_interceptor* interceptor,
	struct kedr_coi_payload* payload)
{
    int result;
    struct payload_elem* elem;
    
	BUG_ON((interceptor->state != interceptor_state_initialized)
		&& (interceptor->state != interceptor_state_started));

    result = mutex_lock_killable(&interceptor->m);
    if(result) return result;
   
    list_for_each_entry(elem, &interceptor->payload_elems, list)
    {
        if(elem->payload != payload) continue;

        if(elem->is_fixed)
        {
            pr_err("Cannot unregister payload %p because it is used "
                "by interceptor now. Please, stop interceptor.",
                payload);
            result = -EBUSY;
            goto out;
        };
        
        list_del(&elem->list);
        
        kfree(elem);
        
        goto out;
    }

    pr_err("Payload %p cannot be unregister because it wasn't registered.",
        payload);
    result = -ENOENT;

out:    
    mutex_unlock(&interceptor->m);
    
    return result;
}


int kedr_coi_interceptor_get_intermediate_info(
	struct kedr_coi_interceptor* interceptor,
	const void* object,
	size_t operation_offset,
	struct kedr_coi_intermediate_info* info)
{
    int result;
    /*
     * This function is allowed to call only by inter mediate operation.
     * Intermediate operation may be called only after successfull 
     * 'watch' call, which in turn may be only in 'started' state of
     * the interceptor.
     */
	BUG_ON(interceptor->state != interceptor_state_started);
    
    result = kedr_coi_instrumentor_get_orig_operation(
        interceptor->instrumentor,
        object,
        operation_offset,
        &info->op_orig);
    
    if(result == 0)
    {
        struct operation_info* operation = interceptor_find_operation(
            interceptor, operation_offset);

        BUG_ON(!operation || !operation->is_intercepted);

        info->pre = operation->pre;
        info->post = operation->post;

        return 0;
    }
    else if(result == 1)
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
	while(!list_empty(&interceptor->foreign_interceptors))
    {
        struct kedr_coi_foreign_interceptor* foreign_interceptor =
            list_first_entry(&interceptor->foreign_interceptors, struct kedr_coi_foreign_interceptor, list);
        
        list_del(&foreign_interceptor->list);
    }
    /*
     * Unregister all payloads which wasn't unregistered before this
     * moment.
     */
    while(!list_empty(&interceptor->payload_elems))
    {
        struct payload_elem* elem = 
            list_first_entry(&interceptor->payload_elems, typeof(*elem), list);
        
        pr_err("Payload %p wasn't unregistered. Unregister it now.",
            elem->payload);
        list_del(&elem->list);
        kfree(elem);
    }
    /*
     *  For control that nobody will access interceptor
     * when it has destroyed.
     */
	interceptor->state = interceptor_state_uninitialized;
    
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
    int result;
    struct kedr_coi_foreign_interceptor* foreign_interceptor;
    
    BUG_ON(interceptor_indirect->state != interceptor_state_initialized);
    
    if(interceptor_indirect->ops->alloc_foreign_interceptor_indirect == NULL)
    {
        pr_err("This interceptor doesn't support foreign interceptors.");
        return NULL;
    }
    
    foreign_interceptor = interceptor_indirect->ops->alloc_foreign_interceptor_indirect(
        interceptor_indirect,
        foreign_operations_field_offset);
    
    if(foreign_interceptor == NULL) return NULL;
    // Initialize foreign interceptor structure
    result = foreign_interceptor_init(foreign_interceptor,
        name,
        intermediate_operations,
        trace_unforgotten_object);
    
    if(result)
    {
        foreign_interceptor->ops->destroy(foreign_interceptor);
        return NULL;
    }
    
    list_add_tail(&foreign_interceptor->list,
        &interceptor_indirect->foreign_interceptors);
    
    return foreign_interceptor;
}


// Internal API of foreign interceptor
int foreign_interceptor_start(
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
            interceptor->replacements,
            interceptor_binded,
            instrumentor_binded);
    
    if(interceptor->instrumentor == NULL) return -EINVAL;
    
    interceptor->state = interceptor_state_started;
    
    return 0;
}

void foreign_interceptor_stop(
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
    /*
     *  Similar to  kedr_coi_get_intermediate_info, this function
     * cannot be called if interceptor is not started.
     */
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
    
    kfree(interceptor->replacements);
    
    interceptor->ops->destroy(interceptor);
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
    
    size_t operations_field_offset;
    size_t operations_struct_size;
};

#define interceptor_indirect(interceptor) container_of(interceptor, struct kedr_coi_interceptor_indirect, base)

/*
 * Foreign indirect interceptor structure.
 */
struct kedr_coi_foreign_interceptor_indirect
{
    struct kedr_coi_foreign_interceptor base;
    
    size_t operations_field_offset;
};

#define foreign_interceptor_indirect(interceptor) container_of(interceptor, struct kedr_coi_foreign_interceptor_indirect, base)

//************Operations of the indirect interceptor******************//

static struct kedr_coi_instrumentor_with_foreign*
interceptor_indirect_op_create_instrumentor_with_foreign(
    struct kedr_coi_interceptor* interceptor,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_interceptor_indirect* interceptor_real =
        interceptor_indirect(interceptor);

    return kedr_coi_instrumentor_with_foreign_create_indirect1(
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
    
    return kedr_coi_instrumentor_create_indirect1(
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

//********Operations of the foreign indirect interceptor**************//

static struct kedr_coi_foreign_instrumentor*
foreign_interceptor_indirect_op_create_foreign_instrumentor(
    struct kedr_coi_foreign_interceptor* interceptor,
    const struct kedr_coi_replacement* replacements,
    struct kedr_coi_interceptor* interceptor_binded,
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded)
{
    struct kedr_coi_foreign_interceptor_indirect* interceptor_real;
    
    interceptor_real = foreign_interceptor_indirect(interceptor);
    
    return kedr_coi_instrumentor_with_foreign_create_foreign_indirect(
        instrumentor_binded,
        interceptor_real->operations_field_offset,
        replacements);
}

static void
foreign_interceptor_indirect_op_destroy(
    struct kedr_coi_foreign_interceptor* interceptor)
{
    struct kedr_coi_foreign_interceptor_indirect* interceptor_real =
        container_of(interceptor, typeof(*interceptor_real), base);
    
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
    size_t operations_field_offset)
{
    struct kedr_coi_foreign_interceptor_indirect* foreign_interceptor =
        kmalloc(sizeof(*foreign_interceptor), GFP_KERNEL);
    
    if(foreign_interceptor == NULL)
    {
        pr_err("Failed to allocate foreign interceptor structure.");
        return NULL;
    }
    
    foreign_interceptor->operations_field_offset = operations_field_offset;
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

// Creation of the indirect interceptor
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


//********Interceptor for objects with direct operations**************//

struct kedr_coi_interceptor_direct
{
    struct kedr_coi_interceptor base;
    
    size_t object_struct_size;
};

#define interceptor_direct(interceptor) container_of(interceptor, struct kedr_coi_interceptor_direct, base)


//*************Operations of the direct interceptor*******************//

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
