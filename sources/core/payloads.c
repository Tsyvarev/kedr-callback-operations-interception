#include "payloads.h"

#include <linux/slab.h>

/*
 * Array of pointers which is allowed to grow.
 * 
 * Its 'elems' field is a NULL-terminated C-array of pointers.
 */
struct parray
{
    void** elems;
    int n_elems;
};


/* Initialize array structure. Array contains no elements. */
static void parray_init(struct parray* array)
{
    array->elems = NULL;
    array->n_elems = 0;
}

/* Reset array. Array contains no elemens.*/
static void parray_reset(struct parray* array)
{
    if(array->n_elems)
    {
        kfree(array->elems);
        array->elems = NULL;
        array->n_elems = 0;
    }
}

/*
 * Add element to array.
 *
 * Return 0 on success, negative error code otherwise.
 */
static int parray_add_elem(struct parray* array, void* elem)
{
    int n_elems_new = array->n_elems + 1;
    void** elems_new = krealloc(array->elems,
        sizeof(*elems_new) * (n_elems_new + 1), GFP_KERNEL);
    if(elems_new == NULL)
    {
        return -ENOMEM;
    }
    
    elems_new[n_elems_new - 1] = elem;
    elems_new[n_elems_new] = NULL;
    
    array->elems = elems_new;
    array->n_elems = n_elems_new;

    return 0;
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
    bool internal_only;
    // Whether operation should be intercepted
    bool is_intercepted;
    // Arrays of pre- and post- handlers
    struct parray pre_handlers;
    struct parray post_handlers;

    // Same for the case when original operation pointer is NULL
    bool default_is_intercepted;
    struct parray default_pre_handlers;
    struct parray default_post_handlers;
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


// Initialize operation information structure
static void operation_info_init(struct operation_info* operation,
    const struct kedr_coi_intermediate* intermediate)
{
    INIT_LIST_HEAD(&operation->list);

    operation->operation_offset = intermediate->operation_offset;

    operation->repl = intermediate->repl;
    operation->group_id = intermediate->group_id;
    operation->internal_only = intermediate->internal_only;

    operation->is_intercepted = false;
    parray_init(&operation->pre_handlers);
    parray_init(&operation->post_handlers);

    operation->default_is_intercepted = false;
    parray_init(&operation->default_pre_handlers);
    parray_init(&operation->default_post_handlers);
}

/* 
 * Clear all interception info, including default interception.
 * Also release any resources concerned with interception.
 */
static void
operation_info_clear_interception(struct operation_info* operation)
{
    if(operation->is_intercepted)
    {
        operation->is_intercepted = false;

        parray_reset(&operation->pre_handlers);
        parray_reset(&operation->post_handlers);
    }

    if(operation->default_is_intercepted)
    {
        operation->default_is_intercepted = false;

        parray_reset(&operation->default_pre_handlers);
        parray_reset(&operation->default_post_handlers);
    }
}

/*
 * Add pre-handler to the array of pre-handlers of operation.
 * 
 * Return 0 on success and negative error code otherwise.
 */
static int
operation_info_add_pre(struct operation_info* operation,
    void* pre, bool external)
{
    int result = parray_add_elem(&operation->pre_handlers, pre);
    if(result)
    {
        pr_err("Failed to add pre handler for operation.");
        return result;
    }

    operation->is_intercepted = true;
    
    if(external)
    {
        result = parray_add_elem(&operation->default_pre_handlers, pre);
        if(result)
        {
            pr_err("Failed to add default pre handler for operation.");
            return result;
        }
        
        operation->default_is_intercepted = true;
    }

    return 0;
}

/*
 * Add post-handler to the array of post-handlers of operation.
 * 
 * Return 0 on success and negative error code otherwise.
 */
static int
operation_info_add_post(struct operation_info* operation,
    void* post, bool external)
{
    int result = parray_add_elem(&operation->post_handlers, post);
    
    if(result)
    {
        pr_err("Failed to add post handler for operation.");
        return result;
    }

    operation->is_intercepted = true;

    if(external)
    {
        result = parray_add_elem(&operation->default_post_handlers, post);
        
        if(result)
        {
            pr_err("Failed to add default post handler for operation.");
            return result;
        }
        operation->default_is_intercepted = true;
    }

    return 0;
}



/*
 * Return information about operation with given offset.
 * 
 * If not found, return NULL.
 */
static struct operation_info* operation_payloads_find_operation(
    struct operation_payloads* payloads, size_t operation_offset)
{
    struct operation_info* operation = NULL;
    list_for_each_entry(operation, &payloads->operations, list)
    {
        if(operation->operation_offset == operation_offset)
            break;
    }
    
    return operation;
}
 
int operation_payloads_init(struct operation_payloads* payloads,
    struct kedr_coi_intermediate* intermediate_operations,
    const char* interceptor_name)
{
    int result;
    
    INIT_LIST_HEAD(&payloads->payload_elems);
    
    INIT_LIST_HEAD(&payloads->operations);
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
            list_add_tail(&operation->list, &payloads->operations);
        }
    }

    mutex_init(&payloads->m);
    payloads->interceptor_name = interceptor_name;
    
    payloads->is_used = 0;

    return 0;

err_operation:
    while(!list_empty(&payloads->operations))
    {
        struct operation_info* operation =
            list_first_entry(&payloads->operations, struct operation_info, list);
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
static int operation_payloads_fix_elem(
    struct operation_payloads* payloads,
    struct payload_elem* elem)
{
    if(elem->payload->mod != NULL)
        if(try_module_get(elem->payload->mod) == 0)
            return -EBUSY;
    
    list_add_tail(&elem->list_used, &payloads->payload_elems_used);

    return 0;
}

/*
 * Actions which should be performed after releasing payload element.
 */
static void operation_payloads_release_elem(
    struct operation_payloads* payloads,
    struct payload_elem* elem)
{
    (void)payloads;
    
    if(elem->payload->mod != NULL)
        module_put(elem->payload->mod);
    
    list_del(&elem->list_used);
}

/*
 * Fix all payload elements.
 * 
 * Return 0 on success and negative error code otherwise.
 */
static int operation_payloads_fix_all(
    struct operation_payloads* payloads)
{
    struct payload_elem* elem;
    list_for_each_entry(elem, &payloads->payload_elems, list)
    {
        if(!operation_payloads_fix_elem(payloads, elem))
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
static void operation_payloads_release_all(
    struct operation_payloads* payloads)
{
    struct payload_elem* elem;
    list_for_each_entry(elem, &payloads->payload_elems, list)
    {
        if(!elem->is_fixed) continue;

        operation_payloads_release_elem(payloads, elem);
        elem->is_fixed = 0;
    }
}

/*
 * Check payload before registration.
 * 
 * Return 0 if payload allowed to register and negative error code
 * otherwise.
 */
static int operation_payloads_check_payload(
    struct operation_payloads* payloads,
    struct kedr_coi_payload* payload)
{
    /*
     *  Verify that payload requires to intercept only known operations
     * and in available variant(external or internal).
     */
    if(payload->pre_handlers)
    {
        struct kedr_coi_pre_handler* pre_handler;
        for(pre_handler = payload->pre_handlers;
            pre_handler->operation_offset != -1;
            pre_handler++)
        {
            struct operation_info* operation =
                operation_payloads_find_operation(payloads,
                    pre_handler->operation_offset);
            
            if(operation == NULL)
            {
                pr_err("Cannot register payload %p for interceptor '%s' because it requires "
                    "to intercept unknown operation with offset %zu.",
                        payload, payloads->interceptor_name, pre_handler->operation_offset);
                return -EINVAL;
            }
            if(pre_handler->external && operation->internal_only)
            {
                pr_err("Cannot register payload %p for interceptor '%s' because it requires "
                    "to externally intercept operation at offset %zu, "
                    "but only internal interception is supported for it.",
                        payload, payloads->interceptor_name, pre_handler->operation_offset);
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
            struct operation_info* operation =
                operation_payloads_find_operation(payloads,
                    post_handler->operation_offset);
            if(operation == NULL)
            {
                pr_err("Cannot register payload %p for interceptor '%s' because it requires "
                    "to intercept unknown operation with offset %zu.",
                        payload, payloads->interceptor_name, post_handler->operation_offset);
                return -EINVAL;
            }
            if(post_handler->external && operation->internal_only)
            {
                pr_err("Cannot register payload %p for interceptor '%s' because it requires "
                    "to externally intercept operation at offset %zu, "
                    "but only internal interception is supported for it.",
                        payload, payloads->interceptor_name, post_handler->operation_offset);
                return -EINVAL;
            }
        }
    }

    return 0;
}

/*
 * Close operations set according to the sence of group_id.
 */
static void close_operations(struct operation_payloads* payloads)
{
    struct operation_info* operation;

    list_for_each_entry(operation, &payloads->operations, list)
    {
        struct operation_info* operation1;

        if(operation->group_id == 0) continue;
        if(!operation->default_is_intercepted) continue;
        list_for_each_entry(operation1, &payloads->operations, list)
        {
            if(operation1->group_id != operation->group_id) continue;
            operation1->default_is_intercepted = true;
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
static int operation_payloads_use_elem(
    struct operation_payloads* payloads,
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
            struct operation_info* operation = operation_payloads_find_operation(
                payloads, pre_handler->operation_offset);

            BUG_ON(operation == NULL);//payloads should be checked when registered
            
            result = operation_info_add_pre(operation,
                pre_handler->func, pre_handler->external);
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
            struct operation_info* operation = operation_payloads_find_operation(
                payloads, post_handler->operation_offset);

            BUG_ON(operation == NULL);//payloads should be checked when registered

            result = operation_info_add_post(operation,
                post_handler->func, post_handler->external);
            if(result) return result;
        }
    }

    return 0;
}

/*
 * Cancel using of all payloads in interception.
 */
static void operation_payloads_unuse_all(
    struct operation_payloads* payloads)
{
    struct operation_info* operation;
    list_for_each_entry(operation, &payloads->operations, list)
    {
        operation_info_clear_interception(operation);
    }
}

/*
 * Create replacements array according to fixed payloads content.
 */
static int
operation_payloads_create_replacements(
    struct operation_payloads* payloads)
{
    struct operation_info* operation;
    int replacements_n = 0;
    int i;
    
    // Count
    list_for_each_entry(operation, &payloads->operations, list)
    {
        if(operation->is_intercepted || operation->default_is_intercepted)
            replacements_n++;
    }
    
    if(replacements_n == 0)
    {
        payloads->replacements = NULL;//nothing to replace
        return 0;
    }
    // Allocate array
    payloads->replacements =
        kmalloc(sizeof(*payloads->replacements) * (replacements_n + 1),
        GFP_KERNEL);
    
    if(payloads->replacements == NULL)
    {
        pr_err("Failed to allocate replacements array");
        return -ENOMEM;
    }
    
    // Filling
    i = 0;
    list_for_each_entry(operation, &payloads->operations, list)
    {
        if(operation->is_intercepted || operation->default_is_intercepted)
        {
            struct kedr_coi_replacement* replacement =
                &payloads->replacements[i];
        
            replacement->operation_offset = operation->operation_offset;
            replacement->repl = operation->repl;
            replacement->mode = operation->default_is_intercepted
                ? operation->is_intercepted ? replace_all : replace_null
                : replace_not_null;
            
            i++;
        }
    }
    BUG_ON(i != replacements_n);
    
    payloads->replacements[replacements_n].operation_offset = -1;
    
    return 0;
}

int operation_payloads_use(struct operation_payloads* payloads,
    int intercept_all)
{
    int result;
	
    struct payload_elem* elem;
    
	result = mutex_lock_killable(&payloads->m);
    if(result)
    {
        pr_err("Failed to acquire mutex on payloads.");
        return result;
    }
    
    if(payloads->is_used)
    {
        pr_err("Interceptor %s attempt to use payloads which are already used.",
            payloads->interceptor_name);
        result = -EBUSY;
        goto out;
    }

    INIT_LIST_HEAD(&payloads->payload_elems_used);
    // Fix payloads for allow their using in interception.
    result = operation_payloads_fix_all(payloads);
    
    if(result) goto out;

    if(intercept_all)
    {
        /* Simply marks all operations as intercepted */
        struct operation_info* operation;
        list_for_each_entry(operation, &payloads->operations, list)
        {
            operation->is_intercepted = true;
            operation->default_is_intercepted = true;
        }
    }

    // Use all fixed payloads for interception.
    list_for_each_entry(elem, &payloads->payload_elems_used, list_used)
    {
        result = operation_payloads_use_elem(payloads, elem);
        
        if(result)
        {
            operation_payloads_unuse_all(payloads);
            operation_payloads_release_all(payloads);
            goto out;
        }
    }
    
    close_operations(payloads);

    result = operation_payloads_create_replacements(payloads);
    
    if(result)
    {
        operation_payloads_unuse_all(payloads);
        operation_payloads_release_all(payloads);
        goto out;
    }

    payloads->is_used = 1;
out:
    mutex_unlock(&payloads->m);

    return result;
}

void operation_payloads_unuse(struct operation_payloads* payloads)
{
    mutex_lock(&payloads->m);
    
    payloads->is_used = 0;
    
    kfree(payloads->replacements);
    payloads->replacements = NULL;
    
    operation_payloads_unuse_all(payloads);
    
    operation_payloads_release_all(payloads);
    
    mutex_unlock(&payloads->m);
}

int operation_payloads_add(
	struct operation_payloads* payloads,
	struct kedr_coi_payload* payload)
{
    int result;
    struct payload_elem* elem;
    
    result = operation_payloads_check_payload(payloads, payload);
    if(result) return result;

    elem = kmalloc(sizeof(*elem), GFP_KERNEL);
    if(elem == NULL)
    {
        pr_err("Failed to allocate registered payload element.");
        return -ENOMEM;
    }

    payload_elem_init(elem, payload);
    
    result = mutex_lock_killable(&payloads->m);
    if(result)
    {
        kfree(elem);
        return result;
    }

    // Check that item wasn't added before.
    {
        struct payload_elem* elem_tmp;
        list_for_each_entry(elem_tmp, &payloads->payload_elems, list)
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
    
    list_add_tail(&elem->list, &payloads->payload_elems);

    mutex_unlock(&payloads->m);
    
    return 0;

err:
    mutex_unlock(&payloads->m);
    kfree(elem);
    return result;
}

int operation_payloads_remove(struct operation_payloads* payloads,
    struct kedr_coi_payload* payload)
{
    int result;
    struct payload_elem* elem;
    
    result = mutex_lock_killable(&payloads->m);
    if(result) return result;
   
    list_for_each_entry(elem, &payloads->payload_elems, list)
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
    mutex_unlock(&payloads->m);
    
    return result;
}

void operation_payloads_destroy(
	struct operation_payloads* payloads)
{
    BUG_ON(payloads->is_used);

    /*
     * Unregister all payloads which wasn't unregistered before this
     * moment.
     */
    while(!list_empty(&payloads->payload_elems))
    {
        struct payload_elem* elem = 
            list_first_entry(&payloads->payload_elems, typeof(*elem), list);
        
        pr_err("Payload %p wasn't unregistered. Unregister it now.",
            elem->payload);
        list_del(&elem->list);
        kfree(elem);
    }
    
    while(!list_empty(&payloads->operations))
    {
        struct operation_info* operation =
            list_first_entry(&payloads->operations, typeof(*operation), list);
        
        list_del(&operation->list);
        kfree(operation);
    }
}

void operation_payloads_get_interception_info(
    struct operation_payloads* payloads, size_t operation_offset,
    int is_default, void* const** pre_p, void* const** post_p)
{
    struct operation_info* operation;
    
    BUG_ON(payloads->is_used == 0);
    
    operation = operation_payloads_find_operation(payloads, operation_offset);
    
    BUG_ON(operation == NULL);
    
    *pre_p = is_default
        ? operation->default_pre_handlers.elems
        : operation->pre_handlers.elems;
    *post_p = is_default
        ? operation->default_post_handlers.elems
        : operation->post_handlers.elems;
}

const struct kedr_coi_replacement* operation_payloads_get_replacements(
    struct operation_payloads* payloads)
{
    BUG_ON(payloads->is_used == 0);
    
    return payloads->replacements;
}