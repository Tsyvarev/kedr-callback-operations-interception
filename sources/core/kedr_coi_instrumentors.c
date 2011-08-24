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

//************* Common instrumentor *****************************

/* 
 *  Callback for deleting currently known objects.
 * 
 * Aside from freeing per-object data, call 'trace_unforgotten_object'
 * for every such object.
 */

static void instrumentor_table_free_elem(
    struct kedr_coi_hash_elem* hash_elem,
    struct kedr_coi_hash_table* objects)
{
    void* object;
    struct kedr_coi_instrumentor* instrumentor;
    struct kedr_coi_instrumentor_object_data* object_data;
    
    struct kedr_coi_object* i_iface_impl;
    struct kedr_coi_instrumentor_impl_iface* i_iface;

    instrumentor = container_of(objects, struct kedr_coi_instrumentor, objects);
    object_data = container_of(hash_elem, struct kedr_coi_instrumentor_object_data, object_elem);
    
    i_iface_impl = instrumentor->i_iface_impl;
    i_iface = container_of(i_iface_impl->interface, typeof(*i_iface), base);
    
    // remove 'const' modifier from key
    object = (void*)hash_elem->key;
    
    i_iface->clean_replacement(
        i_iface_impl,
        object_data,
        object,
        1);
    
    i_iface->free_object_data(
        i_iface_impl,
        object_data);
    
    if(instrumentor->trace_unforgotten_object)
    {
        instrumentor->trace_unforgotten_object(object);
    }
    // for debug
    else
    {
        pr_err("Object %p wasn't removed from the object_data map %p.",
            object, objects);
    }
}


static void kedr_coi_instrumentor_finalize(
    struct kedr_coi_instrumentor* instrumentor)
{
    kedr_coi_hash_table_destroy(&instrumentor->objects,
        instrumentor_table_free_elem);
    
    if(instrumentor->i_iface_impl)
    {
        struct kedr_coi_instrumentor_impl_iface* i_iface =
            container_of(instrumentor->i_iface_impl->interface, typeof(*i_iface), base);
        
        i_iface->destroy_impl(instrumentor->i_iface_impl);
    }
}

static struct kedr_coi_instrumentor_type instrumentor_type =
{
    .finalize = kedr_coi_instrumentor_finalize,
    .struct_offset = 0
};

// Protected constructor
static int kedr_coi_instrumentor_init(
    struct kedr_coi_instrumentor* instrumentor)
{
    int result = kedr_coi_hash_table_init(&instrumentor->objects);
    if(result) return result;
    
    spin_lock_init(&instrumentor->lock);
    
    instrumentor->real_type = &instrumentor_type;
    
    return 0;
}

// Protected method
static void kedr_coi_instrumentor_set_impl(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_object* i_iface_impl)
{
    instrumentor->i_iface_impl = i_iface_impl;
}


// Protected auxiliary methods
// Should be executed with lock taken.
static int
instrumentor_add_object_data(
    struct kedr_coi_instrumentor *instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    return kedr_coi_hash_table_add_elem(&instrumentor->objects,
        &object_data->object_elem);
}

// Should be executed with lock taken.
static void
instrumentor_remove_object_data(
    struct kedr_coi_instrumentor *instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    kedr_coi_hash_table_remove_elem(&instrumentor->objects,
        &object_data->object_elem);
}


// Should be executed with lock taken.
static struct kedr_coi_instrumentor_object_data*
instrumentor_find_object_data(
    struct kedr_coi_instrumentor *instrumentor,
    const void* object)
{
    struct kedr_coi_hash_elem* elem = kedr_coi_hash_table_find_elem(
        &instrumentor->objects, object);
    
    if(elem == NULL) return NULL;
    else return container_of(elem, struct kedr_coi_instrumentor_object_data, object_elem);
}

//**************** Implementation of instrumentor's API ****************

/*
 *  'object_data_new' is allocated per-object data(but not added(!)). 
 * 
 * Should be executed with lock taken.
 * 
 * On error 'object_data_new' should be deleted by the caller.
 */
static int
kedr_coi_instrumentor_watch_internal(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    struct kedr_coi_instrumentor_object_data* object_data_new)
{
    int result;
    struct kedr_coi_instrumentor_object_data* object_data;
    
    struct kedr_coi_object* i_iface_impl = instrumentor->i_iface_impl;
    struct kedr_coi_instrumentor_impl_iface* i_iface =
        container_of(i_iface_impl->interface, typeof(*i_iface), base);
    // Check whether object is already known.
    object_data = instrumentor_find_object_data(instrumentor, object);
    
    if(object_data != NULL)
    {
        // Object is already known, update its per-object data and object's operations
        if(i_iface->update_operations)
        {
            result = i_iface->update_operations(
                i_iface_impl,
                object_data,
                object);
        }
        else
        {
            result = -EEXIST;
        }
        
        if(result < 0) return result;//error
        
        if(result == 0)
        {
            i_iface->free_object_data(
                i_iface_impl,
                object_data_new);

            
            return 1;// indicator of 'update'
        }
        // Need to recreate object data, remove current one
        instrumentor_remove_object_data(instrumentor, object_data);
        i_iface->free_object_data(
            i_iface_impl,
            object_data);
        // Now situation as if object is not known
    }
    
    // Object is not known. Fill new instance of per-object data and add
    // it to the list of known objects.
    result = instrumentor_add_object_data(instrumentor, object_data_new);
    if(result) return result;
    
    result = i_iface->replace_operations(
        i_iface_impl,
        object_data_new,
        object);
    
    if(result)
    {
        instrumentor_remove_object_data(instrumentor, object_data_new);
        return result;
    }
    
    return 0;
}

int
kedr_coi_instrumentor_watch(
    struct kedr_coi_instrumentor* instrumentor,
    void* object)
{
    unsigned long flags;
    int result;
    
    struct kedr_coi_instrumentor_object_data* object_data_new;
    
    struct kedr_coi_object* i_iface_impl = instrumentor->i_iface_impl;
    struct kedr_coi_instrumentor_impl_iface* i_iface =
        container_of(i_iface_impl->interface, typeof(*i_iface), base);

    // Expects that object is firstly watched('likely')
    object_data_new = i_iface->alloc_object_data(i_iface_impl);

    if(object_data_new == NULL)
    {
        // TODO: fallback to the 'update' mode may be appropriate here?
        return -ENOMEM;
    }
    
    kedr_coi_hash_elem_init(&object_data_new->object_elem, object);
    
    spin_lock_irqsave(&instrumentor->lock, flags);
    
    result = kedr_coi_instrumentor_watch_internal(instrumentor,
        object, object_data_new);

    spin_unlock_irqrestore(&instrumentor->lock, flags);
    
    if(result < 0)
    {
        i_iface->free_object_data(
            i_iface_impl,
            object_data_new);
    }

    return result;

}

int
kedr_coi_instrumentor_forget(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    int norestore)
{
    unsigned long flags;
    int result;
    
    struct kedr_coi_instrumentor_object_data* object_data;
    
    struct kedr_coi_object* i_iface_impl = instrumentor->i_iface_impl;
    struct kedr_coi_instrumentor_impl_iface* i_iface =
        container_of(i_iface_impl->interface, typeof(*i_iface), base);

    spin_lock_irqsave(&instrumentor->lock, flags);
    
    object_data = instrumentor_find_object_data(
        instrumentor, object);
    
    if(object_data == NULL)
    {
        // Object is no watched
        
        result = 1;
        goto out;
    }
    
    instrumentor_remove_object_data(instrumentor, object_data);
    
    i_iface->clean_replacement(
        i_iface_impl,
        object_data,
        object,
        norestore);
    
    i_iface->free_object_data(
        i_iface_impl,
        object_data);

    result = 0;
out:
    spin_unlock_irqrestore(&instrumentor->lock, flags);

    return result;
}

void
kedr_coi_instrumentor_destroy(
    struct kedr_coi_instrumentor* instrumentor,
    void (*trace_unforgotten_object)(void* object))
{
    instrumentor->trace_unforgotten_object = trace_unforgotten_object;

    instrumentor->real_type->finalize(instrumentor);
    
    kfree((char*) instrumentor - instrumentor->real_type->struct_offset);
}

//**** Instrumentor for objects with its own operations ****/

static struct kedr_coi_instrumentor_type instrumentor_normal_type =
{
    .finalize = kedr_coi_instrumentor_finalize,
    .struct_offset = offsetof(struct kedr_coi_instrumentor_normal, instrumentor_common)
};
// Constructor
static int kedr_coi_instrumentor_normal_init(
    struct kedr_coi_instrumentor_normal* instrumentor)
{
    int result = kedr_coi_instrumentor_init(
        &instrumentor->instrumentor_common);
    
    if(result) return result;
    
    instrumentor->instrumentor_common.real_type = &instrumentor_normal_type;
    
    return 0;
}


// Protected method
static void kedr_coi_instrumentor_normal_set_impl(
    struct kedr_coi_instrumentor_normal* instrumentor,
    struct kedr_coi_object* in_iface_impl)
{
    kedr_coi_instrumentor_set_impl(
        &instrumentor->instrumentor_common,
        in_iface_impl);

    instrumentor->in_iface_impl = in_iface_impl;
}

//************** Normal instrumentor API *************************/

int
kedr_coi_instrumentor_get_orig_operation(
    struct kedr_coi_instrumentor_normal* instrumentor,
    const void* object,
    size_t operation_offset,
    void** op_orig)
{
    unsigned long flags;
    int result;
    
    struct kedr_coi_instrumentor_object_data* object_data;
    
    struct kedr_coi_object* in_iface_impl = instrumentor->in_iface_impl;
    struct kedr_coi_instrumentor_normal_impl_iface* in_iface =
        container_of(in_iface_impl->interface, typeof(*in_iface), i_iface.base);

    spin_lock_irqsave(&instrumentor->instrumentor_common.lock, flags);
    
    object_data = instrumentor_find_object_data(
        &instrumentor->instrumentor_common,
        object);
    
    if(object_data == NULL)
    {
        // Object not watched
        if(in_iface->get_orig_operation_nodata)
        {
            result = in_iface->get_orig_operation_nodata(
                in_iface_impl,
                object,
                operation_offset,
                op_orig);
        }
        else
        {
            result = -ENOENT;
        }
        if(result == 0) result = 1;// indicator of 'object not watched'
    }
    else
    {
        result = in_iface->get_orig_operation(
            in_iface_impl,
            object_data,
            object,
            operation_offset,
            op_orig);
    }

    spin_unlock_irqrestore(&instrumentor->instrumentor_common.lock, flags);

    return result;
}

struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_normal_create(
    struct kedr_coi_object* in_iface_impl)
{
    struct kedr_coi_instrumentor_normal* instrumentor =
        kmalloc(sizeof(*instrumentor), GFP_KERNEL);
    
    if(instrumentor == NULL)
    {
        pr_err("Failed allocate normal instrumentor structure.");
        return NULL;
    }
    
    if(kedr_coi_instrumentor_normal_init(instrumentor))
    {
        kfree(instrumentor);
        return NULL;
    }
    
    kedr_coi_instrumentor_normal_set_impl(instrumentor, in_iface_impl);
    
    return instrumentor;
}
//********Foreign instrumentor support******************************//
static struct kedr_coi_instrumentor_type instrumentor_with_foreign_type =
{
    .finalize = kedr_coi_instrumentor_finalize,
    .struct_offset = offsetof(struct kedr_coi_instrumentor_with_foreign, _instrumentor_normal.instrumentor_common)
};

// Constructor
static int kedr_coi_instrumentor_with_foreign_init(
    struct kedr_coi_instrumentor_with_foreign* instrumentor)
{
    int result = kedr_coi_instrumentor_normal_init(
        &instrumentor->_instrumentor_normal);
    
    if(result) return result;
    
    instrumentor->_instrumentor_normal.instrumentor_common.real_type = &instrumentor_with_foreign_type;
    
    return 0;
}

// Protected method
static void kedr_coi_instrumentor_with_foreign_set_impl(
    struct kedr_coi_instrumentor_with_foreign* instrumentor,
    struct kedr_coi_object* iwf_iface_impl)
{
    kedr_coi_instrumentor_normal_set_impl(
        &instrumentor->_instrumentor_normal,
        iwf_iface_impl);
    
    instrumentor->iwf_iface_impl = iwf_iface_impl;
}

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create(
    struct kedr_coi_object* iwf_iface_impl)
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
    
    kedr_coi_instrumentor_with_foreign_set_impl(instrumentor, iwf_iface_impl);
    
    return instrumentor;
}

struct kedr_coi_instrumentor_type foreign_instrumentor_type =
{
    .finalize = kedr_coi_instrumentor_finalize,
    .struct_offset = offsetof(struct kedr_coi_foreign_instrumentor, instrumentor_common)
};

//Constructor
static int kedr_coi_foreign_instrumentor_init(
    struct kedr_coi_foreign_instrumentor* instrumentor)
{
    int result = kedr_coi_instrumentor_init(
        &instrumentor->instrumentor_common);
    
    if(result) return result;
    
    instrumentor->instrumentor_binded = NULL;

    instrumentor->instrumentor_common.real_type = &foreign_instrumentor_type;
    
    return 0;
}

// Protected method
static void kedr_coi_foreign_instrumentor_set_impl(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct kedr_coi_object* if_iface_impl,
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded)
{
    kedr_coi_instrumentor_set_impl(
        &instrumentor->instrumentor_common,
        if_iface_impl);
    
    instrumentor->instrumentor_binded = instrumentor_binded;
    instrumentor->if_iface_impl = if_iface_impl;
}

static struct kedr_coi_foreign_instrumentor*
kedr_coi_foreign_instrumentor_create(
    struct kedr_coi_object* if_iface_impl,
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded)
{
    struct kedr_coi_foreign_instrumentor* instrumentor =
        kmalloc(sizeof(*instrumentor), GFP_KERNEL);
    
    if(instrumentor == NULL)
    {
        pr_err("Failed allocate foreign instrumentor structure.");
        return NULL;
    }
    
    if(kedr_coi_foreign_instrumentor_init(instrumentor))
    {
        kfree(instrumentor);
        return NULL;
    }
    
    kedr_coi_foreign_instrumentor_set_impl(instrumentor,
        if_iface_impl, instrumentor_binded);
    
    return instrumentor;
}

//******** Foreign instrumentor API *********************************//

int
kedr_coi_foreign_instrumentor_bind_prototype_with_object(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    void* prototype_object,
    void* object,
    size_t operation_offset,
    void** op_chained)
{
    int result = 0;
    
    struct kedr_coi_instrumentor_object_data* object_data;
    struct kedr_coi_instrumentor_object_data* object_data_new;
    struct kedr_coi_instrumentor_object_data* prototype_data;
    
    struct kedr_coi_instrumentor* instrumentor_common;
    struct kedr_coi_instrumentor* instrumentor_common_binded;
    
    unsigned long flags_foreign, flags_with_foreign;
    
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded =
        instrumentor->instrumentor_binded;
    
    struct kedr_coi_object* if_iface_impl = instrumentor->if_iface_impl;
    struct kedr_coi_foreign_instrumentor_impl_iface* if_iface =
        container_of(if_iface_impl->interface, typeof(*if_iface), i_iface.base);

    struct kedr_coi_object* iwf_iface_impl = instrumentor_binded->iwf_iface_impl;
    struct kedr_coi_instrumentor_with_foreign_impl_iface* iwf_iface =
        container_of(iwf_iface_impl->interface, typeof(*iwf_iface), in_iface.i_iface.base);

    instrumentor_common =
        &instrumentor->instrumentor_common;
    
    instrumentor_common_binded =
        &instrumentor_binded->_instrumentor_normal.instrumentor_common;

    object_data_new =
        iwf_iface->in_iface.i_iface.alloc_object_data(
            iwf_iface_impl);
    // NOTE: doesn't verify object_data at this stage
    
    spin_lock_irqsave(&instrumentor_common_binded->lock, flags_with_foreign);
    spin_lock_irqsave(&instrumentor_common->lock, flags_foreign);

    object_data = instrumentor_find_object_data(instrumentor_common_binded,
        object);
    prototype_data = instrumentor_find_object_data(instrumentor_common,
        prototype_object);

    if(object_data != NULL)
    {
        if(object_data_new)
            iwf_iface->in_iface.i_iface.free_object_data(
                iwf_iface_impl,
                object_data_new);

        result = (prototype_data != NULL)
            ? iwf_iface->update_operations_from_prototype(
                iwf_iface_impl,
                if_iface_impl,
                object_data,
                object,
                prototype_data,
                prototype_object,
                operation_offset,
                op_chained)
            : iwf_iface->chain_operation(
                iwf_iface_impl,
                if_iface_impl,
                object_data,
                object,
                prototype_object,
                operation_offset,
                op_chained);
        goto out;
    }
    else if(prototype_data == NULL)
    {
        if(object_data_new)
            iwf_iface->in_iface.i_iface.free_object_data(
                iwf_iface_impl,
                object_data_new);
        result = if_iface->restore_foreign_operations_nodata(
            if_iface_impl,
            prototype_object,
            object,
            operation_offset,
            op_chained);
        goto out;
    }
    else if(object_data_new == NULL)
    {
        result = if_iface->restore_foreign_operations(
            if_iface_impl,
            prototype_data,
            prototype_object,
            object,
            operation_offset,
            op_chained);
        goto out;

    }
    // Main path    
    
    kedr_coi_hash_elem_init(&object_data_new->object_elem, object);

    result = iwf_iface->replace_operations_from_prototype(
        iwf_iface_impl,
        if_iface_impl,
        object_data_new,
        object,
        prototype_data,
        prototype_object,
        operation_offset,
        op_chained);
    
    if(!result)
        instrumentor_add_object_data(instrumentor_common_binded,
            object_data_new);

out:
    spin_unlock_irqrestore(&instrumentor_common->lock, flags_foreign);
    spin_unlock_irqrestore(&instrumentor_common_binded->lock, flags_with_foreign);
    
    return result;
}

struct kedr_coi_foreign_instrumentor*
kedr_coi_instrumentor_with_foreign_create_foreign_indirect(
    struct kedr_coi_instrumentor_with_foreign* instrumentor,
    size_t operations_field_offset,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_foreign_instrumentor* foreign_instrumentor;
    struct kedr_coi_object* if_iface_impl;
    
    struct kedr_coi_object* iwf_iface_impl = instrumentor->iwf_iface_impl;
    struct kedr_coi_instrumentor_with_foreign_impl_iface* iwf_iface =
        container_of(iwf_iface_impl->interface, typeof(*iwf_iface), in_iface.i_iface.base);


    if(iwf_iface->create_foreign_indirect == NULL)
    {
        pr_err("Instrumentor doesn't support creation indirect foreign instrumentors.");
        return NULL;
    }
    
    if_iface_impl = iwf_iface->create_foreign_indirect(
        iwf_iface_impl,
        operations_field_offset,
        replacements);
    
    if(if_iface_impl == NULL) return NULL;
    
    foreign_instrumentor = kedr_coi_foreign_instrumentor_create(
        if_iface_impl,
        instrumentor);
    
    if(foreign_instrumentor == NULL)
    {
        struct kedr_coi_instrumentor_impl_iface* i_iface =
            container_of(if_iface_impl->interface, typeof(*i_iface), base);
        
        i_iface->destroy_impl(if_iface_impl);
        
        return NULL;
    }
    
    return foreign_instrumentor;
}
