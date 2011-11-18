/*
 * Implementation of instrumentors object hierarchy.
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
 
#include "kedr_coi_instrumentor_internal.h"
#include "kedr_coi_hash_table.h"


#include <linux/slab.h> /* memory allocations */
#include <linux/spinlock.h> /* spinlocks */

//************* Normal instrumentor *****************************

// Wrappers around instrumentor implementation virtual methods
static struct kedr_coi_instrumentor_watch_data*
instrumentor_impl_alloc_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    return iface_impl->interface->alloc_watch_data(iface_impl);
}

static void
instrumentor_impl_free_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    iface_impl->interface->free_watch_data(iface_impl, watch_data);
}

static int
instrumentor_impl_replace_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    return iface_impl->interface->replace_operations(iface_impl,
        watch_data_new, ops_p);
}

static int
instrumentor_impl_update_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    return iface_impl->interface->update_operations
        ? iface_impl->interface->update_operations(iface_impl,
            watch_data, ops_p)
        : -EEXIST;
}

static void
instrumentor_impl_clean_replacement(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    iface_impl->interface->clean_replacement(iface_impl,
        watch_data, ops_p);
}

static void
instrumentor_impl_destroy(struct kedr_coi_instrumentor_impl* iface_impl)
{
    iface_impl->interface->destroy_impl(iface_impl);
}

static void*
instrumentor_impl_get_orig_operation(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void* ops,
    size_t operation_offset)
{
    return iface_impl->interface->get_orig_operation(iface_impl,
        watch_data, ops, operation_offset);
}

static void*
instrumentor_impl_get_orig_operation_nodata(
    struct kedr_coi_instrumentor_impl* iface_impl,
    const void* ops,
    size_t operation_offset)
{
    return iface_impl->interface->get_orig_operation_nodata(iface_impl,
        ops, operation_offset);
}

// Callback for free watch elem
static void free_watch_elem(
    struct kedr_coi_hash_elem* hash_elem,
    struct kedr_coi_hash_table* hash_table)
{
    struct kedr_coi_instrumentor* instrumentor = container_of(
        hash_table, typeof(*instrumentor), hash_table_objects);
    struct kedr_coi_instrumentor_watch_data* watch_data = container_of(
        hash_elem, typeof(*watch_data), hash_elem_object);
    
    instrumentor_impl_clean_replacement(instrumentor->iface_impl,
        watch_data, NULL);
    
    instrumentor_impl_free_watch_data(instrumentor->iface_impl,
        watch_data);
    
    if(instrumentor->trace_unforgotten_watch)
    {
        instrumentor->trace_unforgotten_watch((void*)hash_elem->key,
            instrumentor->user_data);
    }
#ifdef KEDR_COI_DEBUG
    else
    {
        pr_err("Watch %p wasn't removed from the watch_data map %p.",
            hash_elem->key, &instrumentor->hash_table_objects);
    }
#endif

}

static void kedr_coi_instrumentor_finalize(
    struct kedr_coi_instrumentor* instrumentor)
{
    kedr_coi_hash_table_destroy(&instrumentor->hash_table_objects,
        free_watch_elem);

    if(instrumentor->iface_impl)
        instrumentor_impl_destroy(instrumentor->iface_impl);
}

static void kedr_coi_instrumentor_destroy_f(
    struct kedr_coi_instrumentor* instrumentor)
{
    kedr_coi_instrumentor_finalize(instrumentor);
    kfree(instrumentor);
}

// Protected constructor
static int kedr_coi_instrumentor_init(
    struct kedr_coi_instrumentor* instrumentor)
{
    int result;
    
    result = kedr_coi_hash_table_init(&instrumentor->hash_table_objects);
    if(result) return result;
    
    spin_lock_init(&instrumentor->lock);
    
    return 0;
}

// Protected method
static void kedr_coi_instrumentor_set_impl(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    instrumentor->iface_impl = iface_impl;
}


// Protected auxiliary methods
// Should be executed with lock taken.
static int
instrumentor_add_watch_data(
    struct kedr_coi_instrumentor *instrumentor,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    int result;
    
    result = kedr_coi_hash_table_add_elem(
        &instrumentor->hash_table_objects,
        &watch_data->hash_elem_object);
    if(result) return result;
    
    return 0;
}

// Should be executed with lock taken.
static void
instrumentor_remove_watch_data(
    struct kedr_coi_instrumentor *instrumentor,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    kedr_coi_hash_table_remove_elem(&instrumentor->hash_table_objects,
        &watch_data->hash_elem_object);
}


// Should be executed with lock taken.
static struct kedr_coi_instrumentor_watch_data*
instrumentor_find_watch_data(
    struct kedr_coi_instrumentor *instrumentor,
    const void* object)
{
    struct kedr_coi_hash_elem* elem = kedr_coi_hash_table_find_elem(
        &instrumentor->hash_table_objects, object);
    
    if(elem == NULL) return NULL;
    return elem
        ? container_of(elem,
            struct kedr_coi_instrumentor_watch_data,
            hash_elem_object)
        : NULL;
}

//**************** Implementation of instrumentor's API ****************

/*
 *  'watch_data_new' is allocated watch data(but not added(!)). 
 * 
 * Should be executed with lock taken.
 * 
 * On error 'watch_data_new' should be deleted by the caller.
 */
static int
kedr_coi_instrumentor_watch_internal(
    struct kedr_coi_instrumentor* instrumentor,
    const void** ops_p,
    struct kedr_coi_instrumentor_watch_data* watch_data_new)
{
    int result;
    struct kedr_coi_instrumentor_watch_data* watch_data;
    
    struct kedr_coi_instrumentor_impl* iface_impl =
        instrumentor->iface_impl;
    // Check whether object is already known.
    watch_data = instrumentor_find_watch_data(instrumentor,
        (void*)watch_data_new->hash_elem_object.key);
    
    if(watch_data != NULL)
    {
        // Object is already known, update its per-object data and object's operations
        result = instrumentor_impl_update_operations(iface_impl,
            watch_data, ops_p);
        
        if(result < 0) return result;//error
        
        if(result == 0)
        {
            instrumentor_impl_free_watch_data(iface_impl, watch_data_new);
            
            return 1;// indicator of 'update'
        }
        // Need to recreate object data, remove current one
        instrumentor_remove_watch_data(instrumentor, watch_data);
        instrumentor_impl_free_watch_data(iface_impl, watch_data);
        // Now situation as if object is not known
    }
    
    // Object is not known. Fill new instance of watch data and add
    // it to the hash table of known objects.
    result = instrumentor_add_watch_data(instrumentor, watch_data_new);
    if(result) return result;
    
    result = instrumentor_impl_replace_operations(iface_impl,
        watch_data_new,
        ops_p);
    
    if(result)
    {
        instrumentor_remove_watch_data(instrumentor, watch_data_new);
        return result;
    }
    
    return 0;
}

int
kedr_coi_instrumentor_watch(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void** ops_p)
{
    unsigned long flags;
    int result;
    
    struct kedr_coi_instrumentor_watch_data* watch_data_new;
    
    struct kedr_coi_instrumentor_impl* iface_impl =
        instrumentor->iface_impl;

    // Expects that object is firstly watched('likely')
    watch_data_new = instrumentor_impl_alloc_watch_data(iface_impl);

    if(watch_data_new == NULL)
    {
        // TODO: fallback to the 'update' mode may be appropriate here?
        return -ENOMEM;
    }
    
    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_object, object);
    
    spin_lock_irqsave(&instrumentor->lock, flags);
    
    result = kedr_coi_instrumentor_watch_internal(instrumentor,
        ops_p, watch_data_new);

    spin_unlock_irqrestore(&instrumentor->lock, flags);
    
    if(result < 0)
    {
        instrumentor_impl_free_watch_data(iface_impl, watch_data_new);
    }

    return result;
}

int
kedr_coi_instrumentor_forget(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void** ops_p)
{
    unsigned long flags;
    int result;
    
    struct kedr_coi_instrumentor_watch_data* watch_data;
    
    struct kedr_coi_instrumentor_impl* iface_impl =
        instrumentor->iface_impl;

    spin_lock_irqsave(&instrumentor->lock, flags);
    
    watch_data = instrumentor_find_watch_data(instrumentor, object);
    
    if(watch_data == NULL)
    {
        // Object is no watched
        
        result = 1;
        goto out;
    }
    
    instrumentor_remove_watch_data(instrumentor, watch_data);
    
    instrumentor_impl_clean_replacement(iface_impl, watch_data, ops_p);
    instrumentor_impl_free_watch_data(iface_impl, watch_data);
    
    result = 0;
out:
    spin_unlock_irqrestore(&instrumentor->lock, flags);

    return result;
}

void
kedr_coi_instrumentor_destroy(
    struct kedr_coi_instrumentor* instrumentor,
    void (*trace_unforgotten_watch)(void* object, void* user_data),
    void* user_data)
{
    instrumentor->trace_unforgotten_watch = trace_unforgotten_watch;
    instrumentor->user_data = user_data;

    instrumentor->destroy(instrumentor);
}

int
kedr_coi_instrumentor_get_orig_operation(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void* ops,
    size_t operation_offset,
    void** op_orig)
{
    unsigned long flags;
    int result;
    
    struct kedr_coi_instrumentor_watch_data* watch_data;
    
    spin_lock_irqsave(&instrumentor->lock, flags);
    
    watch_data = instrumentor_find_watch_data(instrumentor, object);
    
    if(watch_data == NULL)
    {
        // Object is not watched
        *op_orig = instrumentor_impl_get_orig_operation_nodata(
            instrumentor->iface_impl,
            ops,
            operation_offset);

        result = 1;// indicator of 'object not watched'
    }
    else
    {
        *op_orig = instrumentor_impl_get_orig_operation(
            instrumentor->iface_impl,
            watch_data,
            ops,
            operation_offset);
        
        result = 0;
    }

    spin_unlock_irqrestore(&instrumentor->lock, flags);

    return IS_ERR(*op_orig)? PTR_ERR(*op_orig) : result;
}

struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    struct kedr_coi_instrumentor* instrumentor =
        kmalloc(sizeof(*instrumentor), GFP_KERNEL);
    
    if(instrumentor == NULL)
    {
        pr_err("Failed allocate normal instrumentor structure.");
        return NULL;
    }
    
    if(kedr_coi_instrumentor_init(instrumentor))
    {
        kfree(instrumentor);
        return NULL;
    }
    
    instrumentor->destroy = kedr_coi_instrumentor_destroy_f;
    kedr_coi_instrumentor_set_impl(instrumentor, iface_impl);
    
    return instrumentor;
}
//********Foreign instrumentor support******************************//

// Wrappers around instrumentor implementation virtual methods
static struct kedr_coi_foreign_instrumentor_watch_data*
foreign_instrumentor_impl_alloc_watch_data(
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl)
{
    return foreign_iface_impl->interface->alloc_watch_data(
        foreign_iface_impl);
}

static void
foreign_instrumentor_impl_free_watch_data(
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data)
{
    foreign_iface_impl->interface->free_watch_data(foreign_iface_impl,
        watch_data);
}

static int
foreign_instrumentor_impl_replace_operations(
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    return foreign_iface_impl->interface->replace_operations(
        foreign_iface_impl, watch_data_new, ops_p);
}

static int
foreign_instrumentor_impl_update_operations(
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    return foreign_iface_impl->interface->update_operations
        ? foreign_iface_impl->interface->update_operations(
            foreign_iface_impl, watch_data, ops_p)
        : -EEXIST;
}

static void
foreign_instrumentor_impl_clean_replacement(
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    foreign_iface_impl->interface->clean_replacement(
        foreign_iface_impl, watch_data, ops_p);
}

static void
foreign_instrumentor_impl_destroy(
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl)
{
    foreign_iface_impl->interface->destroy_impl(foreign_iface_impl);
}

static int foreign_instrumentor_impl_restore_foreign_operations(
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
    const void** ops_p,
    size_t operation_offset,
    void** op_orig)
{
    return foreign_iface_impl->interface->restore_foreign_operations(
        foreign_iface_impl, watch_data, ops_p, operation_offset, op_orig);
}

static int foreign_instrumentor_impl_restore_foreign_operations_nodata(
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
    const void** ops_p,
    size_t operation_offset,
    void** op_orig)
{
    return foreign_iface_impl->interface->restore_foreign_operations_nodata(
        foreign_iface_impl, ops_p, operation_offset, op_orig);
}


static struct kedr_coi_foreign_instrumentor_impl*
instrumentor_wf_impl_foreign_instrumentor_impl_create(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_instrumentor_with_foreign_impl_iface* interface_wf =
        container_of(iface_wf_impl->interface, typeof(*interface_wf), base);
    
    return interface_wf->foreign_instrumentor_impl_create(iface_wf_impl,
        replacements);
}

static int
instrumentor_wf_impl_replace_operations_from_foreign(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data_new,
    const void** ops_p,
    struct kedr_coi_foreign_instrumentor_watch_data* foreign_watch_data,
    size_t operation_offset,
    void** op_repl)
{
    struct kedr_coi_instrumentor_with_foreign_impl_iface* interface_wf =
        container_of(iface_wf_impl->interface, typeof(*interface_wf), base);
    
    return interface_wf->replace_operations_from_foreign(
        iface_wf_impl, foreign_iface_impl, watch_data_new, ops_p,
        foreign_watch_data, operation_offset, op_repl);
}

static int
instrumentor_wf_impl_update_operations_from_foreign(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p,
    struct kedr_coi_foreign_instrumentor_watch_data* foreign_watch_data,
    size_t operation_offset,
    void** op_repl)
{
    struct kedr_coi_instrumentor_with_foreign_impl_iface* interface_wf =
        container_of(iface_wf_impl->interface, typeof(*interface_wf), base);
    
    return interface_wf->update_operations_from_foreign(
        iface_wf_impl, foreign_iface_impl, watch_data, ops_p,
        foreign_watch_data, operation_offset, op_repl);
}

static int
instrumentor_wf_impl_chain_operation(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p,
    size_t operation_offset,
    void** op_chained)
{
    struct kedr_coi_instrumentor_with_foreign_impl_iface* interface_wf =
        container_of(iface_wf_impl->interface, typeof(*interface_wf), base);
    
    return interface_wf->chain_operation(
        iface_wf_impl, foreign_iface_impl, watch_data, ops_p,
        operation_offset, op_chained);
}


/* 
 *  Callback for deleting currently known watchings.
 * 
 * Aside from freeing watch data, call 'trace_unforgotten_watch'
 * for every such data.
 */

static void free_foreign_watch_elem(
    struct kedr_coi_hash_elem* hash_elem,
    struct kedr_coi_hash_table* hash_table)
{
    struct kedr_coi_foreign_instrumentor* instrumentor =
        container_of(hash_table, typeof(*instrumentor), hash_table_ids);
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data =
        container_of(hash_elem, typeof(*watch_data), hash_elem_id);
    
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl =
        instrumentor->foreign_iface_impl;

    foreign_instrumentor_impl_clean_replacement(foreign_iface_impl,
        watch_data, NULL);
    
    kedr_coi_hash_table_remove_elem(&instrumentor->hash_table_ties,
        &watch_data->hash_elem_tie);

    foreign_instrumentor_impl_free_watch_data(foreign_iface_impl,
        watch_data);

    if(instrumentor->trace_unforgotten_watch)
    {
        instrumentor->trace_unforgotten_watch((void*)hash_elem->key,
            (void*)watch_data->hash_elem_tie.key, instrumentor->user_data);
    }
#ifdef KEDR_COI_DEBUG
    else
    {
        pr_err("Watch %p wasn't removed from the watch_data map %p.",
            (void*)hash_elem->key, &instrumentor->hash_table_ids);
    }
#endif
}

static void kedr_coi_instrumentor_with_foreign_destroy_f(
    struct kedr_coi_instrumentor* instrumentor)
{
    struct kedr_coi_instrumentor_with_foreign* instrumentor_real =
        container_of(instrumentor, typeof(*instrumentor_real), base);
    
    kedr_coi_instrumentor_finalize(instrumentor);
    kfree(instrumentor_real);
}


// Constructor
static int kedr_coi_instrumentor_with_foreign_init(
    struct kedr_coi_instrumentor_with_foreign* instrumentor)
{
    return kedr_coi_instrumentor_init(&instrumentor->base);
}

// Protected method
static void kedr_coi_instrumentor_with_foreign_set_impl(
    struct kedr_coi_instrumentor_with_foreign* instrumentor,
    struct kedr_coi_instrumentor_impl* iface_wf_impl)
{
    kedr_coi_instrumentor_set_impl(
        &instrumentor->base,
        iface_wf_impl);
    
    instrumentor->iface_wf_impl = iface_wf_impl;
}

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create(
    struct kedr_coi_instrumentor_impl* iface_wf_impl)
{
    struct kedr_coi_instrumentor_with_foreign* instrumentor =
        kmalloc(sizeof(*instrumentor), GFP_KERNEL);
    
    if(instrumentor == NULL)
    {
        pr_err("Failed allocate instrumentor with foreign structure.");
        return NULL;
    }
    
    if(kedr_coi_instrumentor_with_foreign_init(instrumentor))
    {
        kfree(instrumentor);
        return NULL;
    }
    
    instrumentor->base.destroy = kedr_coi_instrumentor_with_foreign_destroy_f;
    kedr_coi_instrumentor_with_foreign_set_impl(instrumentor, iface_wf_impl);
    
    return instrumentor;
}


// Protected constructor
static int kedr_coi_foreign_instrumentor_init(
    struct kedr_coi_foreign_instrumentor* instrumentor)
{
    int result;
    
    result = kedr_coi_hash_table_init(&instrumentor->hash_table_ids);
    if(result) return result;
    
    result = kedr_coi_hash_table_init(&instrumentor->hash_table_ties);
    if(result)
    {
        kedr_coi_hash_table_destroy(&instrumentor->hash_table_ids, NULL);
        return result;
    }
    
    spin_lock_init(&instrumentor->lock);
    
    return 0;
}

// Protected method
static void kedr_coi_foreign_instrumentor_set_impl(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
    struct kedr_coi_instrumentor_with_foreign* binded_instrumentor)
{
    instrumentor->instrumentor_binded = binded_instrumentor;
    instrumentor->foreign_iface_impl = foreign_iface_impl;
}

// Protected auxiliary methods
// Should be executed with lock taken.
static int
foreign_instrumentor_add_watch_data(
    struct kedr_coi_foreign_instrumentor *instrumentor,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data)
{
    int result;
    
    result = kedr_coi_hash_table_add_elem(
        &instrumentor->hash_table_ids,
        &watch_data->hash_elem_id);
    if(result) return result;
    
    result = kedr_coi_hash_table_add_elem(
        &instrumentor->hash_table_ties,
        &watch_data->hash_elem_tie);
    if(result)
    {
        kedr_coi_hash_table_remove_elem(
            &instrumentor->hash_table_ids,
            &watch_data->hash_elem_id);
        return result;
    }
    
    return 0;
}

// Should be executed with lock taken.
static void
foreign_instrumentor_remove_watch_data(
    struct kedr_coi_foreign_instrumentor *instrumentor,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data)
{
    kedr_coi_hash_table_remove_elem(&instrumentor->hash_table_ids,
        &watch_data->hash_elem_id);
    
    kedr_coi_hash_table_remove_elem(&instrumentor->hash_table_ties,
        &watch_data->hash_elem_tie);
}


// Should be executed with lock taken.
static struct kedr_coi_foreign_instrumentor_watch_data*
foreign_instrumentor_find_watch_data(
    struct kedr_coi_foreign_instrumentor *instrumentor,
    const void* id)
{
    struct kedr_coi_hash_elem* elem = kedr_coi_hash_table_find_elem(
        &instrumentor->hash_table_ids, id);

    return elem
        ? container_of(elem,
            struct kedr_coi_foreign_instrumentor_watch_data,
            hash_elem_id)
        : NULL;
}

// Should be executed with lock taken.
static struct kedr_coi_foreign_instrumentor_watch_data*
foreign_instrumentor_find_watch_data_by_tie(
    struct kedr_coi_foreign_instrumentor *instrumentor,
    const void* tie)
{
    struct kedr_coi_hash_elem* elem = kedr_coi_hash_table_find_elem(
        &instrumentor->hash_table_ties, tie);

    return elem
        ? container_of(elem,
            struct kedr_coi_foreign_instrumentor_watch_data,
            hash_elem_tie)
        : NULL;
}


//******** Implementation of foreign instrumentor's API ****************

/*
 *  'watch_data_new' is allocated watch data(but not added(!)). 
 * 
 * Should be executed with lock taken.
 * 
 * On error 'watch_data_new' should be deleted by the caller.
 */
static int
kedr_coi_foreign_instrumentor_watch_internal(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void** ops_p,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data_new)
{
    int result;
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data;
    
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl =
        instrumentor->foreign_iface_impl;
    // Check whether object is already known.
    watch_data = foreign_instrumentor_find_watch_data(instrumentor,
        (void*)watch_data_new->hash_elem_id.key);
    
    if(watch_data != NULL)
    {
        // Id is already known, update its watch data and object's operations
        result = foreign_instrumentor_impl_update_operations(
            foreign_iface_impl, watch_data, ops_p);
        
        if(result < 0) return result;//error
        
        if(result == 0)
        {
            foreign_instrumentor_impl_free_watch_data(
                foreign_iface_impl, watch_data_new);
            
            return 1;// indicator of 'update'
        }
        // Need to recreate object data, remove current one
        foreign_instrumentor_remove_watch_data(instrumentor, watch_data);
        foreign_instrumentor_impl_free_watch_data(
            foreign_iface_impl, watch_data);
        // Now situation as if id is not known
    }
    
    // Id is not known. Fill new instance of watch data and add
    // it to the hash table of known ids.
    result = foreign_instrumentor_add_watch_data(instrumentor,
        watch_data_new);
    if(result) return result;
    
    result = foreign_instrumentor_impl_replace_operations(
        foreign_iface_impl, watch_data_new, ops_p);
    
    if(result)
    {
        foreign_instrumentor_remove_watch_data(instrumentor,
            watch_data_new);
        return result;
    }
    
    return 0;
}

int
kedr_coi_foreign_instrumentor_watch(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* id,
    const void* tie,
    const void** ops_p)
{
    unsigned long flags;
    int result;
    
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data_new;
    
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl =
        instrumentor->foreign_iface_impl;

    // Expects that id is firstly watched('likely')
    watch_data_new = foreign_instrumentor_impl_alloc_watch_data(
        foreign_iface_impl);

    if(watch_data_new == NULL)
    {
        // TODO: fallback to the 'update' mode may be appropriate here?
        return -ENOMEM;
    }
    
    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_id, id);
    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_tie, tie);
    
    spin_lock_irqsave(&instrumentor->lock, flags);
    
    result = kedr_coi_foreign_instrumentor_watch_internal(instrumentor,
        ops_p, watch_data_new);

    spin_unlock_irqrestore(&instrumentor->lock, flags);
    
    if(result < 0)
    {
        foreign_instrumentor_impl_free_watch_data(
            foreign_iface_impl, watch_data_new);
    }

    return result;
}

int
kedr_coi_foreign_instrumentor_forget(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* id,
    const void** ops_p)
{
    unsigned long flags;
    int result;
    
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data;
    
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl =
        instrumentor->foreign_iface_impl;

    spin_lock_irqsave(&instrumentor->lock, flags);
    
    watch_data = foreign_instrumentor_find_watch_data(instrumentor, id);
    
    if(watch_data == NULL)
    {
        // Id is not watched
        result = 1;
        goto out;
    }
    
    foreign_instrumentor_remove_watch_data(instrumentor, watch_data);
    
    foreign_instrumentor_impl_clean_replacement(
        foreign_iface_impl, watch_data, ops_p);
    foreign_instrumentor_impl_free_watch_data(
        foreign_iface_impl, watch_data);
    
    result = 0;
out:
    spin_unlock_irqrestore(&instrumentor->lock, flags);

    return result;
}

//******** Foreign instrumentor API *********************************//

int
kedr_coi_foreign_instrumentor_bind(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* object,
    const void* tie,
    const void** ops_p,
    size_t operation_offset,
    void** op_chained)
{
    int result = 0;
    
    struct kedr_coi_instrumentor_watch_data* watch_data;
    struct kedr_coi_instrumentor_watch_data* watch_data_new;
    struct kedr_coi_foreign_instrumentor_watch_data* foreign_watch_data;
    
    unsigned long flags_f, flags_wf;
    
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded =
        instrumentor->instrumentor_binded;
    
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl =
        instrumentor->foreign_iface_impl;
    struct kedr_coi_instrumentor_impl* iface_wf_impl =
        instrumentor_binded->iface_wf_impl;
    struct kedr_coi_instrumentor_with_foreign_impl_iface* interface_wf =
        container_of(iface_wf_impl->interface, typeof(*interface_wf), base);

    watch_data_new = instrumentor_impl_alloc_watch_data(iface_wf_impl);
    // NOTE: doesn't verify watch_data at this stage
    
    spin_lock_irqsave(&instrumentor_binded->base.lock, flags_wf);
    spin_lock_irqsave(&instrumentor->lock, flags_f);

    watch_data = instrumentor_find_watch_data(
        &instrumentor_binded->base, object);
    foreign_watch_data = foreign_instrumentor_find_watch_data_by_tie(
        instrumentor, tie);

    if(watch_data != NULL)
    {
        if(watch_data_new)
            instrumentor_impl_free_watch_data(iface_wf_impl,
                watch_data_new);

        result = (foreign_watch_data != NULL)
            ? instrumentor_wf_impl_update_operations_from_foreign(
                iface_wf_impl, foreign_iface_impl, watch_data, ops_p,
                foreign_watch_data, operation_offset, op_chained)
            : instrumentor_wf_impl_chain_operation(
                iface_wf_impl, foreign_iface_impl, watch_data, ops_p,
                operation_offset, op_chained);
        goto out;
    }
    else if(foreign_watch_data == NULL)
    {
        if(watch_data_new)
            instrumentor_impl_free_watch_data(
                iface_wf_impl, watch_data_new);
        result = foreign_instrumentor_impl_restore_foreign_operations_nodata(
            foreign_iface_impl, ops_p, operation_offset, op_chained);
        goto out;
    }
    else if(watch_data_new == NULL)
    {
        result = foreign_instrumentor_impl_restore_foreign_operations(
            foreign_iface_impl, foreign_watch_data, ops_p,
            operation_offset, op_chained);
        goto out;

    }
    // Main path    
    
    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_object, object);

    result = instrumentor_wf_impl_replace_operations_from_foreign(
        iface_wf_impl, foreign_iface_impl, watch_data_new, ops_p,
        foreign_watch_data, operation_offset, op_chained);
    
    if(!result)
        instrumentor_add_watch_data(&instrumentor_binded->base,
            watch_data_new);

out:
    spin_unlock_irqrestore(&instrumentor->lock, flags_f);
    spin_unlock_irqrestore(&instrumentor_binded->base.lock, flags_wf);
    
    return result;
}

struct kedr_coi_foreign_instrumentor*
kedr_coi_foreign_instrumentor_create(
    struct kedr_coi_instrumentor_with_foreign* instrumentor,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_foreign_instrumentor* foreign_instrumentor;
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl;
    
    struct kedr_coi_instrumentor_impl* iface_wf_impl =
        instrumentor->iface_wf_impl;

    foreign_iface_impl = instrumentor_wf_impl_foreign_instrumentor_impl_create(
        iface_wf_impl, replacements);
    
    if(foreign_iface_impl == NULL) return NULL;
    
    foreign_instrumentor = kmalloc(sizeof(*foreign_instrumentor), GFP_KERNEL);
    if(foreign_instrumentor == NULL)
    {
        pr_err("Failed to allocate memory for foreign instrumentor");
        foreign_instrumentor_impl_destroy(foreign_iface_impl);
        return NULL;
    }

    if(kedr_coi_foreign_instrumentor_init(foreign_instrumentor))
    {
        kfree(foreign_instrumentor);
        foreign_instrumentor_impl_destroy(foreign_iface_impl);
        return NULL;
    }
    
    kedr_coi_foreign_instrumentor_set_impl(foreign_instrumentor,
        foreign_iface_impl,
        instrumentor);
    
    return foreign_instrumentor;
}

void kedr_coi_foreign_instrumentor_destroy(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    void (*trace_unforgotten_watch)(void* id, void* tie, void* user_data),
    void* user_data)
{
    instrumentor->trace_unforgotten_watch = trace_unforgotten_watch;
    instrumentor->user_data = user_data;
    
    kedr_coi_hash_table_destroy(&instrumentor->hash_table_ids,
        free_foreign_watch_elem);

    if(instrumentor->foreign_iface_impl)
        foreign_instrumentor_impl_destroy(instrumentor->foreign_iface_impl);
    
    kfree(instrumentor);
}
