/*
 * Implementation of the container for normal payloads.
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
 
#include "kedr_coi_payloads_internal.h"

#include <linux/list.h> /* list organization */
#include <linux/slab.h> /* kmalloc */
#include <linux/mutex.h> /* mutex */


//**************** Items registrator ********************************

/*
 * Support mechanism for register/deregister some items
 * and fixing/releasing all registrations.
 */

struct kedr_coi_registrator;

// Container for one registered item
struct kedr_coi_registrator_elem
{
    // Item itself
    void* item;
    // Private fields
    struct list_head list;
    int is_fixed;
};

// Initialize registrator element
static void
kedr_coi_registrator_elem_init(
    struct kedr_coi_registrator_elem* elem,
    void *item);

// Virtual operations of registrator
struct kedr_coi_registrator_operations
{
    /*
     * Called when need to allocate container for new element.
     * 
     * Should return container, initialized for this element.
     */
    struct kedr_coi_registrator_elem* (*alloc_elem)(
        struct kedr_coi_registrator* registrator,
        void* item);
    
    /*
     * Called when container for some element become not needed.
     */
    void (*free_elem)(
        struct kedr_coi_registrator* registrator,
        struct kedr_coi_registrator_elem* elem);
    
    /*
     * Called when item is registered.
     * 
     * Returning not 0 will fail item registration.
     * 
     * May be NULL.
     */
    int (*add_item)(
        struct kedr_coi_registrator* registrator,
        struct kedr_coi_registrator_elem* elem);
    
    /*
     * Called when item is unregistered.
     * 
     * May be NULL.
     */
    void (*remove_item)(
        struct kedr_coi_registrator* registrator,
        struct kedr_coi_registrator_elem* elem);

    /*
     * Called when registered item need to be fixed.
     * 
     * Returning negative value will fail fixing of that item and all other items.
     * 
     * Returning 1 will fail only this item fixing, but not others
     * (kedr_coi_registrator_fix_items may return success in this case).
     * 
     * May be NULL.
     */
    int (*fix_item)(
        struct kedr_coi_registrator* registrator,
        struct kedr_coi_registrator_elem* elem);
    
    /*
     * Called when item is released.
     * 
     * May be NULL.
     */
    void (*release_item)(
        struct kedr_coi_registrator* registrator,
        struct kedr_coi_registrator_elem* elem);
};


/*
 * Virtual object for registration/deregistration objects.
 * 
 * Also support 'fixing' semantics.
 */
struct kedr_coi_registrator
{
    struct list_head elems;
    int is_fixed;
    struct mutex m;
    
    const struct kedr_coi_registrator_operations* r_ops;
};

//****************** API of registrator ***************************
/*
 * Initialize registrator structure
 */
void kedr_coi_registrator_init(
    struct kedr_coi_registrator* registrator,
    const struct kedr_coi_registrator_operations* r_ops);
/*
 * Register item.
 *
 * Return 0 on success or negative error code on fail.
 */
int kedr_coi_registrator_item_register(
    struct kedr_coi_registrator* registrator, void* item);

/*
 * Unregister item.
 *
 * Return 0 on success or negative error code on fail.
 */

int kedr_coi_registrator_item_unregister(
    struct kedr_coi_registrator* registrator, void* item);

/*
 * Fix items in registrator.
 * 
 * Until releasing, no new items may be registered
 * (registration will return -EBUSY)
 * and only items which fail to fix may be unregistered
 * (for other items unregistration will return -EBUSY).
 * 
 * Return 0 on success or negative error on fail.
 */

int kedr_coi_registrator_fix_items(
    struct kedr_coi_registrator* registrator);

/* 
 * Release fixed items, allowing following registrations/unregistrations.
 * 
 * Return 0 on success or negative error on fail.
 * 
 * NOTE: Error may be returning only due to the internal implementation
 * of the registrator.
 */
int kedr_coi_registrator_release_items(
    struct kedr_coi_registrator* registrator);

/*
 * Return not 0 if items are currently fixed.
 */
int kedr_coi_registrator_is_fixed(
    struct kedr_coi_registrator* registrator);

/*
 * Unregister items in the registrator which is currently registered.
 * 
 * Normally, presence of such items is a error in registrator users.
 * 
 * If 'remove_item' is not NULL, it will be called for every such item
 * instead of normal remove_item().
 */
void kedr_coi_registrator_clean_items(
    struct kedr_coi_registrator* registrator,
    void (*remove_item)(struct kedr_coi_registrator*,
                        struct kedr_coi_registrator_elem*));

//********* Payloads container for operations on object itself*********

/*
 * element of the payload registration.
 */
struct payload_elem
{
    struct kedr_coi_registrator_elem base;
    // List of used payloads(constant after fixing them).
    struct list_head list_used;
};

#define payload_elem_from_base(registrator_elem) \
container_of(registrator_elem, struct payload_elem, base)

// Information about one operation
struct operation_info
{
    struct list_head list;// list organization - for 'foreach' implementation.
    size_t operation_offset;
    
    void* repl;
    int group_id;
    // Next fields make a sence only after fixing payloads

    int is_intercepted; //whether interception of this operation is used

    // Next fields has a sence only when is_used is not 0
    // NULL-terminated array of per-handlers or NULL
    void** pre;
    // NULL-terminated array of post-handlers or NULL
    void** post;
    // Private fields
    int n_pre;
    int n_post;
};

static void operation_info_init(struct operation_info* info,
    struct kedr_coi_intermediate* intermediate);

// Mark operation as intercepted
static int
operation_info_intercept(struct operation_info* operation);

// Mark operation as not intercepted and clear interception information.
static void
operation_info_clear_interception(struct operation_info* operation);

static int
operation_info_add_pre(struct operation_info* operation,
    void* pre);

static int
operation_info_add_post(struct operation_info* operation,
    void* pre);


struct kedr_coi_payloads_container
{
    // Registrator for payloads
    struct kedr_coi_registrator registrator;
    // List of all operations available for interception
    struct list_head operations;
    // Used only during payloads fixing
    struct list_head payload_elems_used;
    
    // Used only from fixing payloads to releasing them
    struct kedr_coi_replacement* replacements;

    // Used only during container destroying
    const char* interceptor_name;
};

#define payloads_container_from_registrator(registrator) \
container_of(registrator, struct kedr_coi_payloads_container, registrator)

/*
 * Return information for operation with given offset.
 * 
 * Return NULL if operation is not found.
 */
static struct operation_info*
payloads_container_find_operation(
    struct kedr_coi_payloads_container* container,
    size_t operation_offset);


/*
 *  According to interception information, create replacement pairs.
 */
static int
payloads_container_create_replacements(
    struct kedr_coi_payloads_container* container);

/*
 * Mark operation as intercepted.
 * 
 * Also process operations grouping.
 * 
 * NOTE: Function's effect is not solely revertable!
 */
static int
payloads_container_intercept_operation(
    struct kedr_coi_payloads_container* container,
    struct operation_info* operation);

/*
 * Add all handlers from payload to operations.
 * 
 * NOTE:  Similar to the previous function,
 *        this function also is not revertable.
 */
static int payloads_container_use_payload(
    struct kedr_coi_payloads_container* container,
    struct kedr_coi_payload* payload);

//********Operations of payloads container as registrator**************
static struct kedr_coi_registrator_elem*
payloads_container_ops_alloc_elem(
    struct kedr_coi_registrator* registrator,
    void* item)
{
    struct payload_elem* elem;
    
    elem = kmalloc(sizeof(*elem), GFP_KERNEL);
    if(elem == NULL) return NULL;
    
    kedr_coi_registrator_elem_init(&elem->base, item);
    
    INIT_LIST_HEAD(&elem->list_used);
    
    return &elem->base;
}

static void
payloads_container_ops_free_elem(
    struct kedr_coi_registrator* registrator,
    struct kedr_coi_registrator_elem* elem)
{
    struct payload_elem* elem_real = payload_elem_from_base(elem);
    
    kfree(elem_real);
}

static int
payloads_container_ops_add_item(
    struct kedr_coi_registrator* registrator,
    struct kedr_coi_registrator_elem* elem)
{
    struct kedr_coi_payload* payload;
    struct payload_elem* elem_real;
    struct kedr_coi_payloads_container* container;
    
    container = payloads_container_from_registrator(registrator);
    elem_real = payload_elem_from_base(elem);
    payload = elem->item;

    // Verify that payload requires to intercept only known operations
    if(payload->pre_handlers)
    {
        struct kedr_coi_pre_handler* pre_handler;
        for(pre_handler = payload->pre_handlers;
            pre_handler->operation_offset != -1;
            pre_handler++)
        {
            if(payloads_container_find_operation(container,
                pre_handler->operation_offset) == NULL)
            {
                pr_err("Cannot register payload because it requires to intercept"
                    "unknown operation with offset %zu.", pre_handler->operation_offset);
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
            if(payloads_container_find_operation(container,
                post_handler->operation_offset) == NULL)
            {
                pr_err("Cannot register payload because it requires to intercept "
                    "unknown operation with offset %zu.", post_handler->operation_offset);
                return -EINVAL;
            }
        }
    }

    return 0;
}

static int
payloads_container_ops_fix_item(
    struct kedr_coi_registrator* registrator,
    struct kedr_coi_registrator_elem* elem)
{
    struct kedr_coi_payload* payload;
    struct payload_elem* elem_real;
    struct kedr_coi_payloads_container* container;
    
    container = payloads_container_from_registrator(registrator);
    elem_real = payload_elem_from_base(elem);
    payload = elem->item;
    
    if(payload->mod != NULL)
        if(try_module_get(payload->mod) == 0)
            return -EBUSY;
    
    list_add_tail(&elem_real->list_used, &container->payload_elems_used);
    
    return 0;
}

static void
payloads_container_ops_release_item(
    struct kedr_coi_registrator* registrator,
    struct kedr_coi_registrator_elem* elem)
{
    struct kedr_coi_payload* payload;
    struct payload_elem* elem_real;
    struct kedr_coi_payloads_container* container;
    
    container = payloads_container_from_registrator(registrator);
    elem_real = payload_elem_from_base(elem);
    payload = elem->item;
    
    if(payload->mod != NULL)
        module_put(payload->mod);
    
    list_del(&elem_real->list_used);
}

static struct kedr_coi_registrator_operations payloads_container_ops =
{
    .alloc_elem = payloads_container_ops_alloc_elem,
    .free_elem = payloads_container_ops_free_elem,
    
    .add_item = payloads_container_ops_add_item,

    .fix_item = payloads_container_ops_fix_item,
    .release_item = payloads_container_ops_release_item
};
/************** Implementation of API ********************************/

struct kedr_coi_payloads_container*
kedr_coi_payloads_container_create(
    struct kedr_coi_intermediate* intermediate_operations)
{
    struct kedr_coi_payloads_container* container =
        kmalloc(sizeof(*container), GFP_KERNEL);
    if(container == NULL)
    {
        pr_err("Failed to allocate payloads container.");
        goto err;
    }
    
    INIT_LIST_HEAD(&container->operations);
    if(intermediate_operations)
    {
        struct kedr_coi_intermediate* intermediate;
        
        for(intermediate = intermediate_operations;
            intermediate->operation_offset != -1;
            intermediate++)
        {
            struct operation_info* operation =
                kmalloc(sizeof(*operation), GFP_KERNEL);
            if(operation == NULL)
            {
                pr_err("Failed to allocate information about operation.");
                goto operation_err;
            }
            operation_info_init(operation, intermediate);
            list_add_tail(&operation->list, &container->operations);
        }

    }

    kedr_coi_registrator_init(
        &container->registrator,
        &payloads_container_ops);
    
    container->replacements = NULL;
    
    return container;

operation_err:
    while(!list_empty(&container->operations))
    {
        struct operation_info* operation =
            list_first_entry(&container->operations, struct operation_info, list);
        list_del(&operation->list);
        kfree(operation);
    }
err:
    return NULL;
}

static void
payload_containter_remove_forgotten_item(
    struct kedr_coi_registrator* registrator,
    struct kedr_coi_registrator_elem* elem)
{
    struct kedr_coi_payloads_container* container =
        payloads_container_from_registrator(registrator);

    pr_err("Payload %p for interceptor '%s' wasn't unregistered. Unregister it now.",
        elem->item, container->interceptor_name);
}

void
kedr_coi_payloads_container_destroy(
    struct kedr_coi_payloads_container* container,
    const char* interceptor_name)
{
    BUG_ON(kedr_coi_registrator_is_fixed(&container->registrator));
    
    container->interceptor_name = interceptor_name;
    
    kedr_coi_registrator_clean_items(&container->registrator,
        payload_containter_remove_forgotten_item);
}


int
kedr_coi_payloads_container_register_payload(
    struct kedr_coi_payloads_container* container,
    struct kedr_coi_payload* payload)
{
    int result = kedr_coi_registrator_item_register(
        &container->registrator, payload);
    
    // Output error message if needed
    switch(result)
    {
    case -EBUSY:
        pr_err("Payloads may not be registered after interception starts.");
    }
    
    return result;
}

void
kedr_coi_payloads_container_unregister_payload(
    struct kedr_coi_payloads_container* container,
    struct kedr_coi_payload* payload)
{
    int result = kedr_coi_registrator_item_unregister(
        &container->registrator, payload);
    
    // Output error message if needed
    switch(result)
    {
    case -EBUSY:
        pr_err("Payload is currently used.");
    case -ENOENT:
        pr_err("Payload has not been registered.");
    }
}


int
kedr_coi_payloads_container_fix_payloads(
    struct kedr_coi_payloads_container* container)
{
    int result;

    struct payload_elem* payload_element;
    
    INIT_LIST_HEAD(&container->payload_elems_used);
    
    result = kedr_coi_registrator_fix_items(&container->registrator);

    if(result) return result;
    
    // Store interception handlers for each payload
    list_for_each_entry(payload_element, &container->payload_elems_used, list_used)
    {
        struct kedr_coi_payload* payload = payload_element->base.item;
        result = payloads_container_use_payload(container, payload);
        
        if(result) goto interception_info_err;
    }
    
    result = payloads_container_create_replacements(container);
    if(result) goto replacements_err;
    
    return 0;

replacements_err:

interception_info_err:
    {
        struct operation_info* operation;
        list_for_each_entry(operation, &container->operations, list)
        {
            operation_info_clear_interception(operation);
        }
    }

    kedr_coi_registrator_release_items(&container->registrator);

    return result;
}

const struct kedr_coi_replacement*
kedr_coi_payloads_container_get_replacements(
    struct kedr_coi_payloads_container* container)
{
    BUG_ON(!kedr_coi_registrator_is_fixed(&container->registrator));
    
    return container->replacements;
}

int
kedr_coi_payloads_container_get_handlers(
    struct kedr_coi_payloads_container* container,
    size_t operation_offset,
    void* const** pre_handlers,
    void* const** post_handlers)
{
    struct operation_info* operation;
    BUG_ON(!kedr_coi_registrator_is_fixed(&container->registrator));
    
    operation = payloads_container_find_operation(container, operation_offset);
    
    if(operation)
    {
        *pre_handlers = operation->pre;
        *post_handlers = operation->post;
    }
    else
    {
        pr_err("Unknown operation with offset %zu.", operation_offset);
        *pre_handlers = NULL;
        *post_handlers = NULL;
    }
    return 0;
}

void
kedr_coi_payloads_container_release_payloads(
    struct kedr_coi_payloads_container* container)
{
    struct operation_info* operation;
    if(kedr_coi_registrator_release_items(&container->registrator))
        return; //silent error

    // Free temporary data
    list_for_each_entry(operation, &container->operations, list)
    {
        operation_info_clear_interception(operation);
    }

    kfree(container->replacements);
    container->replacements = NULL;
}

/*************Implementation of auxiliary functions****************/

static void operation_info_init(struct operation_info* operation,
    struct kedr_coi_intermediate* intermediate)
{
    operation->operation_offset = intermediate->operation_offset;
    operation->repl = intermediate->repl;
    operation->group_id = intermediate->group_id;
    
    INIT_LIST_HEAD(&operation->list);
    
    operation->is_intercepted = 0;
}

static int
operation_info_intercept(struct operation_info* operation)
{
    if(operation->is_intercepted) return 0;//already intercepted
    
    operation->pre = NULL;
    operation->post = NULL;

    operation->n_pre = 0;
    operation->n_post = 0;

    operation->is_intercepted = 1;

    return 0;
}

static void
operation_info_clear_interception(struct operation_info* operation)
{
    if(!operation->is_intercepted) return;

    operation->is_intercepted = 0;

    kfree(operation->pre);
    kfree(operation->post);
}

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

static struct operation_info*
payloads_container_find_operation(
    struct kedr_coi_payloads_container* container,
    size_t operation_offset)
{
    struct operation_info* operation;
    list_for_each_entry(operation, &container->operations, list)
    {
        if(operation->operation_offset == operation_offset)
            return operation;
    }
    
    return NULL;
}


static int
payloads_container_create_replacements(
    struct kedr_coi_payloads_container* container)
{
    struct operation_info* operation;
    int replacements_n = 0;
    int i;
    struct list_head* operations = &container->operations;
    // Count
    list_for_each_entry(operation, operations, list)
    {
        if(operation->is_intercepted)
            replacements_n++;
    }
    
    if(replacements_n == 0)
    {
        container->replacements = NULL;//nothing to replace
        return 0;
    }
    
    container->replacements =
        kmalloc(sizeof(*container->replacements) * (replacements_n + 1),
        GFP_KERNEL);
    
    if(container->replacements == NULL)
    {
        pr_err("Failed to allocate replacements array");
        return -ENOMEM;
    }
    
    // Filling
    i = 0;
    list_for_each_entry(operation, operations, list)
    {
        if(operation->is_intercepted)
        {
            struct kedr_coi_replacement* replacement =
                &container->replacements[i];
        
            replacement->operation_offset = operation->operation_offset;
            replacement->repl = operation->repl;
            
            i++;
        }
    }
    BUG_ON(i != replacements_n);
    
    container->replacements[replacements_n].operation_offset = -1;
    
    return 0;
}

static int
payloads_container_intercept_operation(
    struct kedr_coi_payloads_container* container,
    struct operation_info* operation)
{
    int result;
    if(operation->is_intercepted) return 0;
    
    result = operation_info_intercept(operation);
    if(result) return result;
    
    if(operation->group_id != 0)
    {
        int group_id = operation->group_id;
        struct operation_info* operation_tmp;
        list_for_each_entry(operation_tmp, &container->operations, list)
        {
            if(operation_tmp->group_id != group_id) continue;
            if(operation_tmp->is_intercepted) continue;
            result = operation_info_intercept(operation_tmp);
            if(result) return result;
        }
    }

    return 0;
}

static int payloads_container_use_payload(
    struct kedr_coi_payloads_container* container,
    struct kedr_coi_payload* payload)
{
    if(payload->pre_handlers)
    {
        struct kedr_coi_pre_handler* pre_handler;
        for(pre_handler = payload->pre_handlers;
            pre_handler->operation_offset != -1;
            pre_handler++)
        {
            int result;
            struct operation_info* operation =
                payloads_container_find_operation(container, pre_handler->operation_offset);

            BUG_ON(operation == NULL);//payloads should be checked when registered
            result = payloads_container_intercept_operation(container, operation);
            if(result) return result;
            
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
            struct operation_info* operation =
                payloads_container_find_operation(container, post_handler->operation_offset);

            BUG_ON(operation == NULL);//payloads should be checked when registered
            result = payloads_container_intercept_operation(container, operation);
            if(result) return result;
            
            result = operation_info_add_post(operation, post_handler->func);
            if(result) return result;
        }
    }

    return 0;
}

//******************** Implementation of the registrator***************
void
kedr_coi_registrator_elem_init(
    struct kedr_coi_registrator_elem* elem,
    void *item)
{
    elem->item = item;
    INIT_LIST_HEAD(&elem->list);
    elem->is_fixed = 0;
}

void kedr_coi_registrator_init(
    struct kedr_coi_registrator* registrator,
    const struct kedr_coi_registrator_operations* r_ops)
{
    registrator->r_ops = r_ops;
    mutex_init(&registrator->m);
    INIT_LIST_HEAD(&registrator->elems);
    registrator->is_fixed = 0;
}

int kedr_coi_registrator_item_register(
    struct kedr_coi_registrator* registrator, void* item)
{
    int result;
    
    struct kedr_coi_registrator_elem* elem;
    
    elem = registrator->r_ops->alloc_elem(registrator, item);
    
    if(elem == NULL) return -ENOMEM;
    
    result = mutex_lock_killable(&registrator->m);
    if(result)
    {
        registrator->r_ops->free_elem(registrator, item);
        return result;
    }

    if(registrator->is_fixed)
    {
        result = -EBUSY;
        goto err;
    }
    
    // Check that item wasn't added before.
    {
        struct kedr_coi_registrator_elem* elem_tmp;
        list_for_each_entry(elem_tmp, &registrator->elems, list)
        {
            if(elem_tmp->item == item)
            {
                result = -EEXIST;
                goto err;
            }
        }
    }
    
    result = registrator->r_ops->add_item
        ? registrator->r_ops->add_item(registrator, elem)
        : 0;
    
    if(result) goto err;
    
    list_add_tail(&elem->list, &registrator->elems);

    mutex_unlock(&registrator->m);
    
    return 0;

err:
    mutex_unlock(&registrator->m);
    registrator->r_ops->free_elem(registrator, elem);
    return result;
}

// Should be executed with mutex taken
static int kedr_coi_registrator_item_unregister_internal(
    struct kedr_coi_registrator* registrator, void* item)
{
    struct kedr_coi_registrator_elem* elem;
   
    list_for_each_entry(elem, &registrator->elems, list)
    {
        if(elem->item == item)
        {
            if(elem->is_fixed) return -EBUSY;
            
            list_del(&elem->list);
            
            if(registrator->r_ops->remove_item)
                registrator->r_ops->remove_item(registrator, elem);
            
            registrator->r_ops->free_elem(registrator, elem);
            
            return 0;
        }
    }
    
    return -ENOENT;
}

int kedr_coi_registrator_item_unregister(
    struct kedr_coi_registrator* registrator, void* item)
{
    int result;
    
    result = mutex_lock_killable(&registrator->m);
    if(result) return result;
    
    result = kedr_coi_registrator_item_unregister_internal(registrator, item);
    
    mutex_unlock(&registrator->m);
    
    return result;
}

// Should be executed with mutex taken
static int kedr_coi_registrator_fix_items_internal(
    struct kedr_coi_registrator* registrator)
{
    int result;
    struct kedr_coi_registrator_elem* elem;
    
    list_for_each_entry(elem, &registrator->elems, list)
    {
        result = registrator->r_ops->fix_item(registrator, elem);
        if(result < 0) goto err_fix_item;
        if(result > 0) continue; //ignore fixing this element
        
        elem->is_fixed = 1;
    }
    
    registrator->is_fixed = 1;
    return 0;

err_fix_item:
    list_for_each_entry_continue_reverse(elem, &registrator->elems, list)
    {
        if(elem->is_fixed)
        {
            registrator->r_ops->release_item(registrator, elem);
            elem->is_fixed = 0;
        }
    }
    
    return result;
}


int kedr_coi_registrator_fix_items(
    struct kedr_coi_registrator* registrator)
{
    int result;
    
    result = mutex_lock_killable(&registrator->m);
    if(result) return result;

    result = kedr_coi_registrator_fix_items_internal(registrator);

    mutex_unlock(&registrator->m);
    
    return result;
}

int kedr_coi_registrator_release_items(
    struct kedr_coi_registrator* registrator)
{
    int result;
    struct kedr_coi_registrator_elem* elem;
    
    result = mutex_lock_killable(&registrator->m);
    if(result) return result;

    list_for_each_entry(elem, &registrator->elems, list)
    {
        if(elem->is_fixed)
        {
            registrator->r_ops->release_item(registrator, elem);
            elem->is_fixed = 0;
        }
    }
    
    registrator->is_fixed = 0;

    mutex_unlock(&registrator->m);
    
    return 0;
}

void kedr_coi_registrator_clean_items(
    struct kedr_coi_registrator* registrator,
    void (*remove_item)(struct kedr_coi_registrator*,
                        struct kedr_coi_registrator_elem*))
{
    while(!list_empty(&registrator->elems))
    {
        struct kedr_coi_registrator_elem* elem = 
            list_first_entry(&registrator->elems, typeof(*elem), list);
        
        list_del(&elem->list);
        
        if(remove_item)
            remove_item(registrator, elem);
        else if(registrator->r_ops->remove_item)
            registrator->r_ops->remove_item(registrator, elem);
        
        registrator->r_ops->free_elem(registrator, elem);
    }
}

int kedr_coi_registrator_is_fixed(
    struct kedr_coi_registrator* registrator)
{
    return registrator->is_fixed;
}