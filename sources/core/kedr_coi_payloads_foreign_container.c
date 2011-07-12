/*
 * Implementation of the container for foreign payloads.
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
 
#include "kedr_coi_payloads_foreign_internal.h"

#include <linux/list.h> /* list organization */
#include <linux/slab.h> /* kmalloc */
#include <linux/mutex.h> /* mutex */

struct payload_elem
{
    struct list_head list;
    // List of used payloads(constant after fixing them).
    struct list_head list_used;
    
    struct kedr_coi_payload_foreign* payload;
    
    int is_used;
};

static struct payload_elem*
payload_elem_create(struct kedr_coi_payload_foreign* payload);

static void
payload_elem_destroy(struct payload_elem* elem);

static int 
payload_elem_fix(struct payload_elem* elem);

static int 
payload_elem_release(struct payload_elem* elem);


struct kedr_coi_payloads_foreign_container
{
    struct list_head payload_elems;
    
    struct mutex m;
    
    struct kedr_coi_intermediate_foreign* intermediate_operations;
    struct kedr_coi_intermediate_foreign_info* intermediate_info;
    
    int payloads_are_used;
    
    // Next fields have a sence only during using of payloads
    struct list_head payload_elems_used;
    struct kedr_coi_instrumentor_replacement* replacements;
};

/*
 *  According to information from fixed payloads, fill intermediate information.
 */
static int
payload_foreign_containter_init_intermediate_info(
    struct kedr_coi_payloads_foreign_container* container,
    struct kedr_coi_intermediate_foreign_info* intermediate_info);

static void
payload_foreign_containter_destroy_intermediate_info(
    struct kedr_coi_intermediate_foreign_info* intermediate_info);


/*
 *  Create replacement pairs.
 */
static int
payload_foreign_containter_create_replacements(
    struct kedr_coi_payloads_foreign_container* container);


/*
 *  Create replacement pairs in case when no replacements is need -
 * there is no handlers are registered.
 */
static int
payload_foreign_containter_create_replacements_empty(
    struct kedr_coi_payloads_foreign_container* container);

/************** Implementation of API ********************************/

struct kedr_coi_payloads_foreign_container*
kedr_coi_payloads_foreign_container_create(
    struct kedr_coi_intermediate_foreign* intermediate_operations,
    struct kedr_coi_intermediate_foreign_info* intermediate_info)
{
    struct kedr_coi_payloads_foreign_container* container =
        kmalloc(sizeof(*container), GFP_KERNEL);
    if(container == NULL)
    {
        pr_err("Failed to allocate payloads container.");
        return NULL;
    }
    
    INIT_LIST_HEAD(&container->payload_elems);
    
    mutex_init(&container->m);
    
    container->intermediate_operations = intermediate_operations;
    container->intermediate_info = intermediate_info;
    
    container->payloads_are_used = 0;
    
    container->replacements = NULL;
    
    return container;
}

void
kedr_coi_payloads_foreign_container_destroy(
    struct kedr_coi_payloads_foreign_container* container,
        const char* interceptor_name)
{
    BUG_ON(container->payloads_are_used);
    
    while(!list_empty(&container->payload_elems))
    {
        struct payload_elem* elem =
            list_first_entry(&container->payload_elems, struct payload_elem, list);
        pr_err("Payload %p of insterceptor '%s' wasn't unregistered. Unregister it now.",
            elem->payload, interceptor_name);
        BUG_ON(elem->is_used);
        list_del(&elem->list);
        payload_elem_destroy(elem);
    }
    
    mutex_destroy(&container->m);
}

// Shoud be executed with mutex hold.
static int
payload_foreign_container_register_payload_internal(
    struct kedr_coi_payloads_foreign_container* container,
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
kedr_coi_payloads_foreign_container_register_payload(
    struct kedr_coi_payloads_foreign_container* container,
    struct kedr_coi_payload_foreign* payload)
{
    int result;
    
    struct payload_elem* payload_element_new =
        payload_elem_create(payload);

    if(payload_element_new == NULL)
    {
        return -ENOMEM;
    }
    
    if(mutex_lock_killable(&container->m))
    {
        kfree(payload_element_new);
        return -EINTR;
    }
    
    result = payload_foreign_container_register_payload_internal(container,
        payload_element_new);
    
    mutex_unlock(&container->m);
    if(result) kfree(payload_element_new);
    
    return result;
}

// Should be executed with mutex hold.
static void
payload_foreign_container_unregister_payload_internal(
    struct kedr_coi_payloads_foreign_container* container,
    struct kedr_coi_payload_foreign* payload)
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
kedr_coi_payloads_foreign_container_unregister_payload(
    struct kedr_coi_payloads_foreign_container* container,
    struct kedr_coi_payload_foreign* payload)
{
    if(mutex_lock_killable(&container->m))
    {
        BUG();//Unrecoverable error
        return;
    }
    
    payload_foreign_container_unregister_payload_internal(container, payload);

    mutex_unlock(&container->m);
}


struct kedr_coi_instrumentor_replacement*
kedr_coi_payloads_foreign_container_fix_payloads(
    struct kedr_coi_payloads_foreign_container* container)
{
    int result;

    struct payload_elem* payload_element;
    
    if(mutex_lock_killable(&container->m))
    {
        return ERR_PTR(-EINTR);
    }
    // Fixing payloads
    INIT_LIST_HEAD(&container->payload_elems_used);
    list_for_each_entry(payload_element, &container->payload_elems, list)
    {
        if(payload_elem_fix(payload_element) == 0)
            list_add_tail(&payload_element->list_used, &container->payload_elems_used);
    }
    
    result = payload_foreign_containter_init_intermediate_info(container,
        container->intermediate_info);

    if(result) goto intermediate_info_err;
    
    result = container->intermediate_info->on_create_handlers
        ?  payload_foreign_containter_create_replacements(container)
        :  payload_foreign_containter_create_replacements_empty(container);
    if(result) goto replacements_err;
    
    container->payloads_are_used = 1;
    
    mutex_unlock(&container->m);

    return container->replacements;

replacements_err:
    payload_foreign_containter_destroy_intermediate_info(container->intermediate_info);
intermediate_info_err:

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
kedr_coi_payloads_foreign_container_release_payloads(
    struct kedr_coi_payloads_foreign_container* container)
{
    struct payload_elem* payload_element;

    if(mutex_lock_killable(&container->m))
    {
        // Unrecoverable error(cannot report about it)
        BUG();
        return;
    }


    // Release payloads
    while(!list_empty(&container->payload_elems_used))
    {
        payload_element = list_first_entry(&container->payload_elems_used,
            struct payload_elem, list_used);
        payload_elem_release(payload_element);
        list_del(&payload_element->list_used);
    }

    container->payloads_are_used = 0;
    
    // Free temporary data
    kfree(container->replacements);
    container->replacements = NULL;

    payload_foreign_containter_destroy_intermediate_info(container->intermediate_info);

    mutex_unlock(&container->m);
}

/*************Implementation of auxiliary functions****************/


struct payload_elem*
payload_elem_create(struct kedr_coi_payload_foreign* payload)
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
 *  According to information from fixed payloads, fill intermediate information.
 */
int
payload_foreign_containter_init_intermediate_info(
    struct kedr_coi_payloads_foreign_container* container,
    struct kedr_coi_intermediate_foreign_info* intermediate_info)
{
    struct payload_elem* payload_element;
    int on_create_handlers_n = 0;
    int i;
    // Count
    list_for_each_entry(payload_element, &container->payload_elems_used, list_used)
    {
        struct kedr_coi_payload_foreign* payload = 
            payload_element->payload;
        
        if(payload->on_create_handlers)
        {
            kedr_coi_handler_foreign_t* on_create_handler;
            for(on_create_handler = payload->on_create_handlers;
                *on_create_handler !=  NULL;
                on_create_handler++)
                on_create_handlers_n++;
        }
        
    }

    if(on_create_handlers_n == 0)
    {
        // This is an indicator that no replacements should be made
        intermediate_info->on_create_handlers = NULL;
        return 0;
    }
    
    intermediate_info->on_create_handlers =
        kmalloc(sizeof(*intermediate_info->on_create_handlers) * (on_create_handlers_n + 1),
        GFP_KERNEL);
    
    if(intermediate_info->on_create_handlers == NULL)
    {
        pr_err("Failed to allocate array of handlers");
        return -ENOMEM;
    }
    
    // Filling
    i = 0;
    list_for_each_entry(payload_element, &container->payload_elems_used, list_used)
    {
        struct kedr_coi_payload_foreign* payload = 
            payload_element->payload;
        
        if(payload->on_create_handlers)
        {
            kedr_coi_handler_foreign_t* on_create_handler;
            for(on_create_handler = payload->on_create_handlers;
                *on_create_handler !=  NULL;
                on_create_handler++)
                {
                    intermediate_info->on_create_handlers[i] = *on_create_handler;
                    i++;
                }
        }
        
    }
    BUG_ON(i != on_create_handlers_n);
    
    intermediate_info->on_create_handlers[on_create_handlers_n] = NULL;

    return 0;
}

void
payload_foreign_containter_destroy_intermediate_info(
    struct kedr_coi_intermediate_foreign_info* intermediate_info)
{
    kfree(intermediate_info->on_create_handlers);
    intermediate_info->on_create_handlers = NULL;
}


/*
 *  Create replacement pairs.
 */
int
payload_foreign_containter_create_replacements(
    struct kedr_coi_payloads_foreign_container* container)
{
    struct kedr_coi_intermediate_foreign* intermediate_operation;
    int replacements_n = 0;
    int i;
    // Count
    for(intermediate_operation = container->intermediate_operations;
        intermediate_operation->operation_offset != -1;
        intermediate_operation++)
        replacements_n++;
        
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
    for(intermediate_operation = container->intermediate_operations;
        intermediate_operation->operation_offset != -1;
        intermediate_operation++)
        {
            struct kedr_coi_instrumentor_replacement* replacement =
                &container->replacements[i];
            
            replacement->operation_offset = intermediate_operation->operation_offset;
            replacement->repl = intermediate_operation->repl;
            
            i++;
        }
    BUG_ON(i != replacements_n);
    
    container->replacements[replacements_n].operation_offset = -1;

    return 0;
}

int
payload_foreign_containter_create_replacements_empty(
    struct kedr_coi_payloads_foreign_container* container)
{
    container->replacements =
        kmalloc(sizeof(*container->replacements), GFP_KERNEL);
    if(container->replacements == NULL)
    {
        pr_err("Failed to allocate replacements");
        return -ENOMEM;
    }

    container->replacements[0].operation_offset = -1;
    
    return 0;
}
