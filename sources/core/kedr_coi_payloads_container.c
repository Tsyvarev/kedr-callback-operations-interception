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

/* Information about interception of concrete operation*/
struct operation_interception_info
{
    // list organization
    struct list_head list;
    // offset of this operation
    size_t operation_offset;
    // NULL-terminated array of pre-functions
    void** pre;
    // NULL-terminated array of post-functions
    void** post;

    // Private fields
    int n_pre;
    int n_post;
};

static struct operation_interception_info*
operation_interception_info_create(size_t operation_offset);

static void
operation_interception_info_destroy(struct operation_interception_info* elem);

static int
operation_interception_info_add_pre(struct operation_interception_info* elem,
    void* pre);

static int
operation_interception_info_add_post(struct operation_interception_info* elem,
    void* post);


/* Information about interception info for all operations*/
struct operations_interception_info
{
    struct list_head elems;
};

static struct operations_interception_info*
operations_interception_info_create(void);

static void
operations_interception_info_destroy(struct operations_interception_info* info);

/*
 *  Return information of operation, corresponded to the given offset.
 * 
 * If such information is absent, allocate and add it.
 * 
 * On error return ERR_PTR().
 */

static struct operation_interception_info*
operations_interception_info_get_elem(struct operations_interception_info* info,
    size_t operation_offset);

/* 
 * Note: on error initial state of 'info' is not restored.
 */
static int
operations_interception_info_add_payload(struct operations_interception_info* info,
    struct kedr_coi_payload* payload);

struct payload_elem
{
    struct list_head list;
    // List of used payloads(constant after fixing them).
    struct list_head list_used;
    
    struct kedr_coi_payload* payload;
    
    int is_used;
};

static struct payload_elem*
payload_elem_create(struct kedr_coi_payload* payload);

static void
payload_elem_destroy(struct payload_elem* elem);

static int 
payload_elem_fix(struct payload_elem* elem);

static int 
payload_elem_release(struct payload_elem* elem);


struct kedr_coi_payload_container
{
    struct list_head payload_elems;
    
    struct mutex m;
    
    struct kedr_coi_intermediate* intermediate_operations;
    
    int payloads_are_used;
    
    // Next fields have a sence only during using of payloads
    struct list_head payload_elems_used;
    struct kedr_coi_instrumentor_replacement* replacements;
    struct operations_interception_info *interception_info;
};

/*
 * Return intermediate information for operation with given offset.
 * 
 * Return NULL if operation is not found.
 * 
 * Because function works with final fields of container,
 * locking is not required.
 */
static struct kedr_coi_intermediate*
payload_container_get_intermediate(
    struct kedr_coi_payload_container* container,
    size_t operation_offset);


/*
 *  According to interception information, create replacement pairs.
 */
static int
payload_containter_create_replacements(
    struct kedr_coi_payload_container* container,
    struct operations_interception_info* interception_info);

/************** Implementation of API ********************************/

struct kedr_coi_payload_container*
kedr_coi_payload_container_create(
    struct kedr_coi_intermediate* intermediate_operations)
{
    struct kedr_coi_payload_container* container =
        kmalloc(sizeof(*container), GFP_KERNEL);
    if(container == NULL)
    {
        pr_err("Failed to allocate payloads container.");
        return NULL;
    }
    
    INIT_LIST_HEAD(&container->payload_elems);
    
    mutex_init(&container->m);
    
    container->intermediate_operations = intermediate_operations;
    
    container->payloads_are_used = 0;
    
    container->replacements = NULL;
    container->interception_info = NULL;
    
    return container;
}

void
kedr_coi_payload_container_destroy(
    struct kedr_coi_payload_container* container,
    const char* interceptor_name)
{
    BUG_ON(container->payloads_are_used);
    
    while(!list_empty(&container->payload_elems))
    {
        struct payload_elem* elem =
            list_first_entry(&container->payload_elems, struct payload_elem, list);
        pr_err("Payload %p for interceptor '%s' wasn't unregistered. Unregister it now.",
            elem->payload, interceptor_name);
        BUG_ON(elem->is_used);
        list_del(&elem->list);
        payload_elem_destroy(elem);
    }
    
    mutex_destroy(&container->m);
}

// Shoud be executed with mutex hold.
static int
container_register_payload_internal(
    struct kedr_coi_payload_container* container,
    struct payload_elem* payload_element_new)
{
    struct payload_elem* payload_element;
    
    if(container->payloads_are_used)
    {
        pr_err("Cannot register payload because payloads already in use.");
        return -EBUSY;
    }
    
    list_for_each_entry(payload_element, &container->payload_elems, list)
    {
        if(payload_element->payload == payload_element_new->payload)
        {
            pr_err("Payload already registered.");
            return -EEXIST;
        }
    }
    
    list_add_tail(&payload_element_new->list, &container->payload_elems);
    
    return 0;
}

int
kedr_coi_payload_container_register_payload(
    struct kedr_coi_payload_container* container,
    struct kedr_coi_payload* payload)
{
    int result;
    
    struct payload_elem* payload_element_new;
    
    // Verify that payload requires to intercept only known operations
    if(payload->handlers_pre)
    {
        struct kedr_coi_handler_pre* handler_pre;
        for(handler_pre = payload->handlers_pre;
            handler_pre->operation_offset != -1;
            handler_pre++)
        {
            if(payload_container_get_intermediate(container,
                handler_pre->operation_offset) == NULL)
            {
                pr_err("Cannot register payload because it requires to intercept"
                    "unknown operation with offset %zu", handler_pre->operation_offset);
                return -EINVAL;
            }
        }
    }
    
    if(payload->handlers_post)
    {
        struct kedr_coi_handler_post* handler_post;
        for(handler_post = payload->handlers_post;
            handler_post->operation_offset != -1;
            handler_post++)
        {
            if(payload_container_get_intermediate(container,
                handler_post->operation_offset) == NULL)
            {
                pr_err("Cannot register payload because it requires to intercept "
                    "unknown operation with offset %zu", handler_post->operation_offset);
                return -EINVAL;
            }
        }
    }


    payload_element_new = payload_elem_create(payload);

    if(payload_element_new == NULL)
    {
        return -ENOMEM;
    }
    
    if(mutex_lock_killable(&container->m))
    {
        kfree(payload_element_new);
        return -EINTR;
    }
    
    result = container_register_payload_internal(container,
        payload_element_new);
    
    mutex_unlock(&container->m);
    if(result) kfree(payload_element_new);
    
    return result;
}

// Should be executed with mutex hold.
static void
container_unregister_payload_internal(
    struct kedr_coi_payload_container* container,
    struct kedr_coi_payload* payload)
{
    struct payload_elem* payload_element;
    
    list_for_each_entry(payload_element, &container->payload_elems, list)
    {
        if(payload_element->payload == payload)
        {
            if(payload_element->is_used)
            {
                pr_err("Payload is currently used.");
                return;
            }
            list_del(&payload_element->list);

            kfree(payload_element);
            return;
        }
    }
    
    pr_err("Payload has not been registered.");
}

void
kedr_coi_payload_container_unregister_payload(
    struct kedr_coi_payload_container* container,
    struct kedr_coi_payload* payload)
{
    if(mutex_lock_killable(&container->m))
    {
        BUG();//Unrecoverable error
        return;
    }
    
    container_unregister_payload_internal(container, payload);

    mutex_unlock(&container->m);
}


struct kedr_coi_instrumentor_replacement*
kedr_coi_payload_container_fix_payloads(
    struct kedr_coi_payload_container* container)
{
    int result;

    struct payload_elem* payload_element;
    
    if(mutex_lock_killable(&container->m))
    {
        return ERR_PTR(-EINTR);
    }
    // First iteration - fixing payloads
    INIT_LIST_HEAD(&container->payload_elems_used);
    list_for_each_entry(payload_element, &container->payload_elems, list)
    {
        if(payload_elem_fix(payload_element) == 0)
            list_add_tail(&payload_element->list_used, &container->payload_elems_used);
    }
    
    // Second iteration - store interception handlers for each operation
    container->interception_info =
        operations_interception_info_create();
    if(container->interception_info == NULL)
    {
        result = -ENOMEM;
        goto interception_info_create_err;
    }
    list_for_each_entry(payload_element, &container->payload_elems_used, list_used)
    {
        result = operations_interception_info_add_payload(container->interception_info,
            payload_element->payload);
        
        if(result) goto interception_info_err;
    }
    
    result = payload_containter_create_replacements(container,
        container->interception_info);
    if(result) goto replacements_err;
    
    container->payloads_are_used = 1;
    
    mutex_unlock(&container->m);

    return container->replacements;

replacements_err:

interception_info_err:
    operations_interception_info_destroy(container->interception_info);

interception_info_create_err:
    
    // Rollback fixing payloads
    while(!list_empty(&container->payload_elems_used))
    {
        payload_element = list_first_entry(&container->payload_elems_used,
            struct payload_elem, list_used);
        payload_elem_release(payload_element);
        list_del(&payload_element->list_used);
    }

    mutex_unlock(&container->m);
    return ERR_PTR(result);
}

void
kedr_coi_payload_container_release_payloads(
    struct kedr_coi_payload_container* container)
{
    struct payload_elem* payload_element;

    if(mutex_lock_killable(&container->m))
    {
        // Unrecoverable error(cannot report about it)
        BUG();
        return;
    }
    container->payloads_are_used = 0;

    // Release payloads
    while(!list_empty(&container->payload_elems_used))
    {
        payload_element = list_first_entry(&container->payload_elems_used,
            struct payload_elem, list_used);
        payload_elem_release(payload_element);
        list_del(&payload_element->list_used);
    }

    // Free temporary data
    kfree(container->replacements);
    container->replacements = NULL;
    
    operations_interception_info_destroy(container->interception_info);
    container->interception_info = NULL;
    
    mutex_unlock(&container->m);
}

/*************Implementation of auxiliary functions****************/

struct operation_interception_info*
operation_interception_info_create(size_t operation_offset)
{
    struct operation_interception_info* elem =
        kmalloc(sizeof(*elem), GFP_KERNEL);
    
    if(elem == NULL)
    {
        pr_err("Cannot allocate interception info.");
        return NULL;
    }
    
    INIT_LIST_HEAD(&elem->list);
    
    elem->operation_offset = operation_offset;
    
    elem->n_pre = 0;
    elem->n_post = 0;
    
    elem->pre = NULL;
    elem->post = NULL;
    
    return elem;
}

void
operation_interception_info_destroy(struct operation_interception_info* elem)
{
    kfree(elem->pre);
    kfree(elem->post);
    
    kfree(elem);
}

int
operation_interception_info_add_pre(struct operation_interception_info* elem,
    void* pre)
{
    void** pre_new = krealloc(elem->pre, sizeof(*elem->pre) * (elem->n_pre + 2),
        GFP_KERNEL);
    if(pre_new == NULL)
    {
        pr_err("Failed to allocate array of pre handlers for operation.");
        return -ENOMEM;
    }
    
    elem->pre = pre_new;

    elem->pre[elem->n_pre] = pre;
    
    elem->n_pre++;
    elem->pre[elem->n_pre] = NULL;
    
    return 0;
}

int
operation_interception_info_add_post(struct operation_interception_info* elem,
    void* post)
{
    void** post_new = krealloc(elem->post, sizeof(*elem->post) * (elem->n_post + 2),
        GFP_KERNEL);
    if(post_new == NULL)
    {
        pr_err("Failed to allocate array of post handlers for operation.");
        return -ENOMEM;
    }
    
    elem->post = post_new;

    elem->post[elem->n_post] = post;
    
    elem->n_post++;
    elem->post[elem->n_post] = NULL;
    
    return 0;
}


struct operations_interception_info*
operations_interception_info_create(void)
{
    struct operations_interception_info* info =
        kmalloc(sizeof(*info), GFP_KERNEL);
    if(info == NULL)
    {
        pr_err("Failed to allocate interception information about operations.");
        return NULL;
    }
    
    INIT_LIST_HEAD(&info->elems);
    
    return info;
}

void
operations_interception_info_destroy(struct operations_interception_info* info)
{
    while(!list_empty(&info->elems))
    {
        struct operation_interception_info* elem =
            list_first_entry(&info->elems, struct operation_interception_info, list);
        list_del(&elem->list);
        operation_interception_info_destroy(elem);
    }
    kfree(info);
}

/*
 *  Return information of operation, corresponded to the given offset.
 * 
 * If such information is absent, allocate and add it.
 * 
 * On error return ERR_PTR().
 */

struct operation_interception_info*
operations_interception_info_get_elem(struct operations_interception_info* info,
    size_t operation_offset)
{
    struct operation_interception_info* elem;
    list_for_each_entry(elem, &info->elems, list)
    {
        if(elem->operation_offset == operation_offset) return elem;
    }
    
    elem = operation_interception_info_create(operation_offset);
    if(elem == NULL) return ERR_PTR(-ENOMEM);

    list_add_tail(&elem->list, &info->elems);
    
    return elem;
}

int
operations_interception_info_add_payload(struct operations_interception_info* info,
    struct kedr_coi_payload* payload)
{
    if(payload->handlers_pre)
    {
        struct kedr_coi_handler_pre* handler_pre;
        for(handler_pre = payload->handlers_pre;
            handler_pre->operation_offset != -1;
            handler_pre++)
        {
            int result;
            struct operation_interception_info* elem =
                operations_interception_info_get_elem(info, handler_pre->operation_offset);
            if(elem == NULL) return -ENOMEM;
            result = operation_interception_info_add_pre(elem, handler_pre->func);
            if(result) return result;
        }
    }
    
    if(payload->handlers_post)
    {
        struct kedr_coi_handler_post* handler_post;
        for(handler_post = payload->handlers_post;
            handler_post->operation_offset != -1;
            handler_post++)
        {
            int result;
            struct operation_interception_info* elem =
                operations_interception_info_get_elem(info, handler_post->operation_offset);
            if(elem == NULL) return -ENOMEM;
            result = operation_interception_info_add_post(elem, handler_post->func);
            if(result) return result;
        }
    }

    return 0;
}

struct payload_elem*
payload_elem_create(struct kedr_coi_payload* payload)
{
    struct payload_elem* payload_element =
        kmalloc(sizeof(*payload_element), GFP_KERNEL);
    
    if(payload_element == NULL)
    {
        pr_err("Failed to allocate payload element.");
        return NULL;
    }
    
    INIT_LIST_HEAD(&payload_element->list);
    INIT_LIST_HEAD(&payload_element->list_used);
    
    payload_element->is_used = 0;
    payload_element->payload = payload;
    
    return payload_element;
}

void
payload_elem_destroy(struct payload_elem* elem)
{
    kfree(elem);
}


int 
payload_elem_fix(struct payload_elem* elem)
{
    BUG_ON(elem->is_used);
    
    if(elem->payload->mod != NULL)
        if(try_module_get(elem->payload->mod) == 0)
            return -EBUSY;
    
    elem->is_used = 1;
    
    return 0;
}

int 
payload_elem_release(struct payload_elem* elem)
{
    BUG_ON(!elem->is_used);
    if(elem->payload->mod != NULL)
        module_put(elem->payload->mod);
    
    elem->is_used = 0;
    
    return 0;
}


/*
 * Return intermediate information for operation with given offset.
 * 
 * Return NULL if operation is not found.
 * 
 * Because function works with final fields of container,
 * locking is not required.
 */
struct kedr_coi_intermediate*
payload_container_get_intermediate(
    struct kedr_coi_payload_container* container,
    size_t operation_offset)
{
    if(container->intermediate_operations)
    {
        struct kedr_coi_intermediate* intermediate_operation;
        for(intermediate_operation = container->intermediate_operations;
            intermediate_operation->operation_offset != -1;
            intermediate_operation++)
        {
            if(intermediate_operation->operation_offset == operation_offset)
                return intermediate_operation;
        }
    }

    return NULL;
}


/*
 *  According to interception information, create replacement pairs.
 */
int
payload_containter_create_replacements(
    struct kedr_coi_payload_container* container,
    struct operations_interception_info* interception_info)
{
    struct operation_interception_info* elem;
    int replacements_n = 0;
    int i;
    // Count
    list_for_each_entry(elem, &interception_info->elems, list)
    {
        replacements_n++;
    }
    
    container->replacements =
        kmalloc(sizeof(*container->replacements) * (replacements_n + 1),
        GFP_KERNEL);
    
    if(container->replacements == NULL)
    {
        pr_err("Failed to allocate replacements");
        return -ENOMEM;
    }
    
    // Filling
    i = 0;
    list_for_each_entry(elem, &interception_info->elems, list)
    {
        struct kedr_coi_instrumentor_replacement* replacement =
            &container->replacements[i];
        struct kedr_coi_intermediate* intermediate_operation =
            payload_container_get_intermediate(container, elem->operation_offset);
        BUG_ON(intermediate_operation == NULL);
        
        replacement->operation_offset = elem->operation_offset;
        replacement->repl = intermediate_operation->repl;
        
        intermediate_operation->info->pre = elem->pre;
        intermediate_operation->info->post = elem->post;
        
        i++;
    }
    BUG_ON(i != replacements_n);
    
    container->replacements[replacements_n].operation_offset = -1;

    return 0;
}
