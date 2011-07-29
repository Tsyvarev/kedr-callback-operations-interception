/*
 * Implementation of instrumentors of different types.
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

/*
 * Global map for instrumentors.
 * 
 * Prevents using different instrumentors for same operations.
 * Otherwise this may lead to infinitive loops in operations chaining.
 */

// Key for the global map - embedded structure
struct kedr_coi_global_key
{
    struct kedr_coi_hash_elem hash_elem;
};

// Initialize key for global map
static void
kedr_coi_global_key_init(
    struct kedr_coi_global_key* key,
    const void* value);

// Initialize global map
static int kedr_coi_global_map_init(void);

/*
 * Add key to the global map.
 * 
 * Return 0 on success and negative error code on fail.
 * 
 * If key with same value is already added, return -EBUSY.
 */
static int kedr_coi_global_map_add_key_unique(struct kedr_coi_global_key* key);

/*
 * Remove key from the global map.
 */
static void kedr_coi_global_map_remove_key(struct kedr_coi_global_key* key);

// Destroy global map
static void kedr_coi_global_map_destroy(void);

//*************Common instrumentor structure*****************************

struct kedr_coi_instrumentor_object_data
{
    struct kedr_coi_hash_elem hash_elem;
    struct kedr_coi_global_key global_key;
};

// helper for alloc_object_data() operation
static void
instrumentor_object_data_init(
    struct kedr_coi_instrumentor_object_data* object_data,
    const void* object,
    const void* global_key)
{
    kedr_coi_hash_elem_init(&object_data->hash_elem, object);
    kedr_coi_global_key_init(&object_data->global_key, global_key);
}
// 'operations' struct for instrumentor
struct kedr_coi_instrumentor_operations
{
    struct kedr_coi_instrumentor_object_data* (*alloc_object_data)(
        struct kedr_coi_instrumentor*, const void* object);

    void (*free_object_data)(
        struct kedr_coi_instrumentor*,
        struct kedr_coi_instrumentor_object_data*);
    
    int (*replace_operations)(
        struct kedr_coi_instrumentor*,
        struct kedr_coi_instrumentor_object_data*,
        void* object);
    
    void (*restore_operations)(
        struct kedr_coi_instrumentor*,
        struct kedr_coi_instrumentor_object_data*,
        void* object);
    
    /*
     *  Called when 'watch' is called for object for which we already watch for.
     * 
     *  Should update object_data and operations in the object for
     * continue watch for an object.
     * 
     * If need to recreate object_data, should restore operations in the object
     * and return 1.
     */
    int (*update_operations)(
        struct kedr_coi_instrumentor*,
        struct kedr_coi_instrumentor_object_data*,
        void* object);
    
    int (*get_orig_operations)(
        struct kedr_coi_instrumentor*,
        struct kedr_coi_instrumentor_object_data*,
        const void** ops_orig);
    
    int (*get_orig_operations_nodata)(
        struct kedr_coi_instrumentor*,
        const void* object,
        const void** ops_orig);
    
    void (*free_instance)(struct kedr_coi_instrumentor*);
};


/* Instrumentor as virtual object */
struct kedr_coi_instrumentor
{
    struct kedr_coi_hash_table objects;
    spinlock_t lock;
    
    const struct kedr_coi_instrumentor_operations* i_ops;
    // used only while instrumentor is being deleted
    void (*trace_unforgotten_object)(void* object);
};

// Initialize instrumentor's base object(helper for instrumentor constructor)
static int
instrumentor_common_init(struct kedr_coi_instrumentor *instrumentor,
    const struct kedr_coi_instrumentor_operations* ops)
{
    int result = kedr_coi_hash_table_init(&instrumentor->objects);
    if(result) return result;
    
    spin_lock_init(&instrumentor->lock);
    
    instrumentor->i_ops = ops;
    
    return 0;
}

static int
instrumentor_add_object_data(
    struct kedr_coi_instrumentor *instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    int result = kedr_coi_hash_table_add_elem(&instrumentor->objects,
        &object_data->hash_elem);
    
    if(result) return result;
    
    result = kedr_coi_global_map_add_key_unique(&object_data->global_key);
    
    if(result)
    {
        kedr_coi_hash_table_remove_elem(&instrumentor->objects,
            &object_data->hash_elem);
        return result;
    }
    
    return 0;
}


static void
instrumentor_remove_object_data(
    struct kedr_coi_instrumentor *instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    kedr_coi_global_map_remove_key(&object_data->global_key);
    kedr_coi_hash_table_remove_elem(&instrumentor->objects,
        &object_data->hash_elem);
}


static struct kedr_coi_instrumentor_object_data*
instrumentor_find_object_data(
    struct kedr_coi_instrumentor *instrumentor,
    const void* object)
{
    struct kedr_coi_hash_elem* elem = kedr_coi_hash_table_find_elem(
        &instrumentor->objects, object);
    
    if(elem == NULL) return NULL;
    else return container_of(elem, struct kedr_coi_instrumentor_object_data, hash_elem);
}

//**************** Implementation of instrumentor's API ****************

/*
 *  'object_data_new' is allocated per-object data(but not added(!)). 
 * 
 * Should be executed with lock taken.
 */
static int
kedr_coi_instrumentor_watch_internal(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    struct kedr_coi_instrumentor_object_data* object_data_new)
{
    int result;
    // Check whether object is already known.
    struct kedr_coi_instrumentor_object_data* object_data =
        instrumentor_find_object_data(instrumentor, object);
    
    if(object_data != NULL)
    {
        // Object is already known, update its per-object data and object's operations
        if(instrumentor->i_ops->update_operations)
        {
            result = instrumentor->i_ops->update_operations(
                instrumentor,
                object_data,
                object);
        }
        else
        {
            result = -EEXIST;
        }
        if(result <= 0)
        {
       
            instrumentor->i_ops->free_object_data(
                instrumentor, object_data_new);

            if(result) return result;//error
            
            return 1;// indicator of 'update'
        }
        // Need to recreate object data, remove current one
        instrumentor_remove_object_data(instrumentor, object_data);
        instrumentor->i_ops->free_object_data(instrumentor, object_data);
        // Now situation as if object is not known
    }
    
    // Object is not known. Fill new instance of per-object data and add
    // it to the list of known objects.
    result = instrumentor_add_object_data(instrumentor, object_data_new);
    if(result)
    {
        instrumentor->i_ops->free_object_data(
            instrumentor, object_data_new);
        return result;
    }
    
    result = instrumentor->i_ops->replace_operations(
        instrumentor,
        object_data_new,
        object);
    
    if(result)
    {
        instrumentor_remove_object_data(instrumentor, object_data_new);
        instrumentor->i_ops->free_object_data(
            instrumentor, object_data_new);
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
    
    // Expects that object is firstly watched('likely')
    object_data_new = instrumentor->i_ops->alloc_object_data(
        instrumentor, object);

    if(object_data_new == NULL)
    {
        // TODO: fallback to the 'update' mode may be appropriate here?
        return -ENOMEM;
    }
    
    spin_lock_irqsave(&instrumentor->lock, flags);
    
    result = kedr_coi_instrumentor_watch_internal(instrumentor,
        object, object_data_new);

    spin_unlock_irqrestore(&instrumentor->lock, flags);

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
    
    if(!norestore)
    {
        instrumentor->i_ops->restore_operations(instrumentor,
            object_data,
            object);
    }
    
    instrumentor->i_ops->free_object_data(instrumentor, object_data);

    result = 0;
out:
    spin_unlock_irqrestore(&instrumentor->lock, flags);

    return result;
}

int
kedr_coi_instrumentor_get_orig_operations(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void** ops_orig)
{
    unsigned long flags;
    int result;
    
    struct kedr_coi_instrumentor_object_data* object_data;

    spin_lock_irqsave(&instrumentor->lock, flags);
    
    object_data = instrumentor_find_object_data(
        instrumentor,
        object);
    
    if(object_data == NULL)
    {
        // Object not watched
        if(instrumentor->i_ops->get_orig_operations_nodata)
        {
            result = instrumentor->i_ops->get_orig_operations_nodata(
                instrumentor, object, ops_orig);
        }
        else
        {
            result = -ENOENT;
        }
        if(result == 0) result = 1;// indicator of 'object not watched'
    }
    else
    {
        result = instrumentor->i_ops->get_orig_operations(
            instrumentor, object_data, ops_orig);
    }

    spin_unlock_irqrestore(&instrumentor->lock, flags);

    return result;
}

/* 
 *  Callback for deleting currently known objects.
 * 
 * Aside with freeing per-object data, call 'trace_unforgotten_object'
 * for every such object.
 */

static void instrumentor_table_free_elem(
    struct kedr_coi_hash_elem* hash_elem,
    struct kedr_coi_hash_table* objects)
{
    void* object;
    struct kedr_coi_instrumentor* instrumentor;
    struct kedr_coi_instrumentor_object_data* object_data;
    
    instrumentor = container_of(objects, struct kedr_coi_instrumentor, objects);
    object_data = container_of(hash_elem, struct kedr_coi_instrumentor_object_data, hash_elem);
    
    // remove 'const' modifier from key
    object = (void*)hash_elem->key;
    
    kedr_coi_global_map_remove_key(&object_data->global_key);
    
    instrumentor->i_ops->free_object_data(
        instrumentor,
        object_data);
    
    if(instrumentor->trace_unforgotten_object)
    {
        instrumentor->trace_unforgotten_object(object);
    }
}


void
kedr_coi_instrumentor_destroy(struct kedr_coi_instrumentor* instrumentor,
    void (*trace_unforgotten_object)(void* object))
{
    instrumentor->trace_unforgotten_object = trace_unforgotten_object;
    kedr_coi_hash_table_destroy(&instrumentor->objects,
        instrumentor_table_free_elem);
    
    instrumentor->i_ops->free_instance(instrumentor);
}

//*********** Instrumentor which does not instrument at all*************
/*
 * Such instrumentor is a fallback for instrumentor's constructors
 * when nothing to replace.
 * 
 * There are two types of such instrumentor - direct and indirect one.
 * These types differ only in object->global key correspondence, and,
 * consequently, in constructors/destructors.
 */

struct kedr_coi_instrumentor_empty;
struct instrumentor_empty_operations
{
    const void* (*get_object_global_key)(
        struct kedr_coi_instrumentor_empty* instrumentor,
        const void* object);
    void (*free_instance)(struct kedr_coi_instrumentor_empty* instrumentor);
};

struct kedr_coi_instrumentor_empty
{
    struct kedr_coi_instrumentor base;
    const struct instrumentor_empty_operations* ie_ops;
};

#define instrumentor_empty(instrumentor) container_of(instrumentor, struct kedr_coi_instrumentor_empty, base)

static struct kedr_coi_instrumentor_object_data*
instrumentor_empty_ops_alloc_object_data(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object)
{
    struct kedr_coi_instrumentor_empty* instrumentor_real;
    struct kedr_coi_instrumentor_object_data* object_data;
    
    instrumentor_real = instrumentor_empty(instrumentor);
    object_data = kmalloc(sizeof(*object_data), GFP_KERNEL);
    
    if(object_data == NULL) return NULL;
    
    instrumentor_object_data_init(object_data, object,
        instrumentor_real->ie_ops->get_object_global_key(instrumentor_real, object));
    
    return object_data;
}

static void instrumentor_empty_ops_free_object_data(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    kfree(object_data);
}

static int instrumentor_empty_ops_replace_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    return 0;// nothing to replace
}

static void instrumentor_empty_ops_restore_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
}


static int instrumentor_empty_ops_update_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    return 0;// up-to-date
}

static void instrumentor_empty_ops_free_instance(
    struct kedr_coi_instrumentor* instrumentor)
{
    struct kedr_coi_instrumentor_empty* instrumentor_real =
        instrumentor_empty(instrumentor);

    instrumentor_real->ie_ops->free_instance(instrumentor_real);
}

struct kedr_coi_instrumentor_operations instrumentor_empty_ops =
{
    .alloc_object_data = instrumentor_empty_ops_alloc_object_data,
    .free_object_data = instrumentor_empty_ops_free_object_data,
    
    .replace_operations = instrumentor_empty_ops_replace_operations,
    .restore_operations = instrumentor_empty_ops_restore_operations,
    .update_operations = instrumentor_empty_ops_update_operations,
    
    .free_instance = instrumentor_empty_ops_free_instance
};

static int
instrumentor_empty_init(struct kedr_coi_instrumentor_empty* instrumentor,
    const struct instrumentor_empty_operations* ie_ops)
{
    int result = instrumentor_common_init(&instrumentor->base,
        &instrumentor_empty_ops);
    if(result) return result;

    instrumentor->ie_ops = ie_ops;
    return 0;
}



//********* Instrumentor for indirect operations (mostly used)*********/

/*
 * Per-operations data for instrumentor.
 * 
 * 'ops_orig' and 'ops_repl' fields exist only for convinience
 * (values of these fields are hash element keys)
 */
struct instrumentor_indirect_ops_data
{
    //key - replaced operations
    struct kedr_coi_hash_elem hash_elem_ops_repl;
    //key - original operations
    struct kedr_coi_hash_elem hash_elem_ops_orig;
    
    const void* ops_orig;
    void* ops_repl;
    
    int refs;
};


/*
 *  Per-object data for instrumentor.
 * 
 * Contain reference to per-operations data plus key for global map.
 */
struct instrumentor_indirect_object_data
{
    struct kedr_coi_instrumentor_object_data base;
    // Pointer to the operations data, which is referenced by this object
    struct instrumentor_indirect_ops_data* ops_data;
};

#define object_data_indirect(object_data) container_of(object_data, struct instrumentor_indirect_object_data, base)

// Object type for indirect instrumentor.
struct kedr_coi_instrumentor_indirect
{
    struct kedr_coi_instrumentor base;

    // hash table with replaced operations as keys
    struct kedr_coi_hash_table hash_table_ops_repl;
    // hash table with original operations as keys
    struct kedr_coi_hash_table hash_table_ops_orig;
    
    size_t operations_struct_size;
    size_t operations_field_offset;
    // NOTE: this field is not NULL(without replacements empty variant of instrumentor is created)
    const struct kedr_coi_replacement* replacements;
};


// Convert pointer to the common instrumentor to the pointer to indirect one.
#define instrumentor_indirect(instrumentor) container_of(instrumentor, struct kedr_coi_instrumentor_indirect, base)

// Return global key for object
static inline const void*
instrumentor_indirect_object_key_global(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const void* object)
{
    return (const char*)object + instrumentor->operations_field_offset;
}

// Return operations struct for the given object.
static inline const void*
instrumentor_indirect_object_ops_get(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const void* object)
{
    void* operations_offset =
        (char*) object + instrumentor->operations_field_offset;
    return *(const void**)operations_offset;
}

// Set operations for the given object
static inline void
instrumentor_indirect_object_ops_set(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    void* object,
    const void* ops)
{
    void* operations_offset =
        (char*) object + instrumentor->operations_field_offset;
    *(const void**)operations_offset = ops;
}


/*
 * Allocate instance of per-operations data and fill its some fields.
 * 
 * NOTE: May be called in atomic context.
 */
static struct instrumentor_indirect_ops_data*
instrumentor_indirect_alloc_ops_data(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const void* ops_orig)
{
    void* ops_repl;
    struct instrumentor_indirect_ops_data* ops_data =
        kmalloc(sizeof(*ops_data), GFP_ATOMIC);
    
    if(ops_data == NULL) return NULL;
    
    ops_repl = kmalloc(instrumentor->operations_struct_size, GFP_ATOMIC);
    
    if(ops_repl == NULL)
    {
        kfree(ops_data);
        return NULL;
    }
    
    kedr_coi_hash_elem_init(&ops_data->hash_elem_ops_orig, ops_orig);
    kedr_coi_hash_elem_init(&ops_data->hash_elem_ops_repl, ops_repl);
    
    ops_data->ops_orig = ops_orig;
    ops_data->ops_repl = ops_repl;
    
    ops_data->refs = 1;
    
    return ops_data;
}

// Free instance of per-operations data.
static void
instrumentor_indirect_free_ops_data(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    struct instrumentor_indirect_ops_data* ops_data)
{
    kfree(ops_data->ops_repl);
    kfree(ops_data);
}

/*
 * Fill instance of per-operations data according to the replacement
 * information of the interceptor.
 */
static void
instrumentor_indirect_fill_ops_data(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    struct instrumentor_indirect_ops_data* ops_data)
{
    const struct kedr_coi_replacement* replacement;
    char* ops_repl = ops_data->ops_repl;

    memcpy(ops_repl, ops_data->ops_orig,
        instrumentor->operations_struct_size);
    
    for(replacement = instrumentor->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        void* operation_addr = ops_repl + replacement->operation_offset;
        *((void**)operation_addr) = replacement->repl;
    }
}

/* 
 * Decrement refcount of the instance of per-operations data.
 * 
 * If refcount become 0, remove operations wrapper of this instance
 * from the table of known wrappers and finally free instance.
 */
static void
instrumentor_indirect_unref_ops_data(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    struct instrumentor_indirect_ops_data* ops_data)
{
    if(--ops_data->refs != 0) return;
    
    kedr_coi_hash_table_remove_elem(&instrumentor->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig);
    
    kedr_coi_hash_table_remove_elem(&instrumentor->hash_table_ops_repl,
        &ops_data->hash_elem_ops_repl);

    instrumentor_indirect_free_ops_data(instrumentor, ops_data);
}


/* 
 * Increment refcount of the instance of per-operations data.
 */
static void
instrumentor_indirect_ref_ops_data(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    struct instrumentor_indirect_ops_data* ops_data)
{
    ops_data->refs ++;
}

/*
 * Return per-operations data which corresponds to the given operations
 * and 'ref' these data.
 * 
 * If such data don't exist, create new one and return it.
 */

static struct instrumentor_indirect_ops_data*
instrumentor_indirect_get_ops_data(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const void* object_ops)
{
    struct instrumentor_indirect_ops_data* ops_data;
    struct kedr_coi_hash_elem* elem_tmp;
    
    // Look for existed per-operations data for that operations
    elem_tmp = kedr_coi_hash_table_find_elem(
        &instrumentor->hash_table_ops_orig,
        object_ops);
    if(elem_tmp != NULL)
    {
        ops_data = container_of(elem_tmp, typeof(*ops_data), hash_elem_ops_orig);
        instrumentor_indirect_ref_ops_data(instrumentor, ops_data);
        return ops_data;
    }
    
    elem_tmp = kedr_coi_hash_table_find_elem(
        &instrumentor->hash_table_ops_repl,
        object_ops);
    if(elem_tmp != NULL)
    {
        ops_data = container_of(elem_tmp, typeof(*ops_data), hash_elem_ops_repl);
        instrumentor_indirect_ref_ops_data(instrumentor, ops_data);
        return ops_data;
    }

    // Per-operations data for these operations are not exist, create new one
    ops_data = instrumentor_indirect_alloc_ops_data(instrumentor,
        object_ops);

    if(ops_data == NULL) return NULL;
    
    instrumentor_indirect_fill_ops_data(instrumentor, ops_data);
    
    if(kedr_coi_hash_table_add_elem(
        &instrumentor->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig))
    {
        instrumentor_indirect_free_ops_data(instrumentor, ops_data);
        return NULL;
    }
    
    if(kedr_coi_hash_table_add_elem(
        &instrumentor->hash_table_ops_repl,
        &ops_data->hash_elem_ops_repl))
    {
        kedr_coi_hash_table_remove_elem(
            &instrumentor->hash_table_ops_orig,
            &ops_data->hash_elem_ops_orig);
        instrumentor_indirect_free_ops_data(instrumentor, ops_data);
        return NULL;
    }

    return ops_data;
}

//*************Operations of the indirect instrumentor****************

static struct kedr_coi_instrumentor_object_data*
instrumentor_indirect_ops_alloc_object_data(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object)
{
    struct instrumentor_indirect_ops_data* ops_data;
    
    struct instrumentor_indirect_object_data* object_data;
    const void* object_ops;
    
    struct kedr_coi_instrumentor_indirect* instrumentor_real =
        instrumentor_indirect(instrumentor);

    // TODO: kmem_cache may be more appropriate
    object_data = kmalloc(sizeof(*object_data), GFP_KERNEL);
    if(object_data == NULL) return NULL;
    
    object_ops = instrumentor_indirect_object_ops_get(instrumentor_real, object);
    
    ops_data = instrumentor_indirect_get_ops_data(instrumentor_real,
        object_ops);
    
    if(ops_data == NULL)
    {
        kfree(object_data);
        return NULL;
    }

    object_data->ops_data = ops_data;

    instrumentor_object_data_init(&object_data->base,
        object,
        instrumentor_indirect_object_key_global(instrumentor_real, object));

    return &object_data->base;
}

// Free instance of per-object data.
static void
instrumentor_indirect_ops_free_object_data(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    struct kedr_coi_instrumentor_indirect* instrumentor_real;
    struct instrumentor_indirect_object_data* object_data_real;

    instrumentor_real = instrumentor_indirect(instrumentor);
    object_data_real = object_data_indirect(object_data);
    
    instrumentor_indirect_unref_ops_data(instrumentor_real,
        object_data_real->ops_data);
    
    kfree(object_data_real);
}


static int
instrumentor_indirect_ops_replace_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct kedr_coi_instrumentor_indirect* instrumentor_real;
    struct instrumentor_indirect_object_data* object_data_real;
    
    const void* ops_repl;
    
    BUG_ON(object_data == NULL);
    
    instrumentor_real = instrumentor_indirect(instrumentor);
    object_data_real = object_data_indirect(object_data);
    
    BUG_ON(object_data_real->ops_data == NULL);
    
    ops_repl = object_data_real->ops_data->ops_repl;
    
    instrumentor_indirect_object_ops_set(instrumentor_real, object, ops_repl);
    
    return 0;
}

static void
instrumentor_indirect_ops_restore_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct kedr_coi_instrumentor_indirect* instrumentor_real;
    struct instrumentor_indirect_object_data* object_data_real;
    
    const void* object_ops;
    
    instrumentor_real = instrumentor_indirect(instrumentor);
    object_data_real = object_data_indirect(object_data);
    
    object_ops = instrumentor_indirect_object_ops_get(instrumentor_real, object);
    
    if(object_ops == object_data_real->ops_data->ops_repl)
        instrumentor_indirect_object_ops_set(instrumentor_real, object,
            object_data_real->ops_data->ops_orig);
}

static int
instrumentor_indirect_ops_update_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct kedr_coi_instrumentor_indirect* instrumentor_real;
    struct instrumentor_indirect_object_data* object_data_real;
    struct instrumentor_indirect_ops_data* ops_data;
    struct instrumentor_indirect_ops_data* ops_data_new;
    
    const void* object_ops;
    
    instrumentor_real = instrumentor_indirect(instrumentor);
    object_data_real = object_data_indirect(object_data);
    
    object_ops = instrumentor_indirect_object_ops_get(instrumentor_real,
        object);
    
    ops_data = object_data_real->ops_data;
    
    if(object_ops == ops_data->ops_repl)
    {
        //up-to-date
        return 0;
    }
    
    if(object_ops == ops_data->ops_orig)
    {
        //set replaced operations again
        instrumentor_indirect_object_ops_set(instrumentor_real, object,
            ops_data->ops_repl);
        return 0;
    }
    
    ops_data_new = instrumentor_indirect_get_ops_data(instrumentor_real,
        object_ops);
    
    if(ops_data_new == NULL) return -ENOMEM;
    
    instrumentor_indirect_object_ops_set(instrumentor_real, object,
        ops_data_new->ops_repl);
    
    object_data_real->ops_data = ops_data_new;
    instrumentor_indirect_unref_ops_data(instrumentor_real, ops_data);
    
    return 0;
}

static int instrumentor_indirect_ops_get_orig_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    const void** ops_orig)
{
    struct instrumentor_indirect_object_data* object_data_real;
    
    object_data_real = object_data_indirect(object_data);
    
    *ops_orig = object_data_real->ops_data->ops_orig;
    
    return 0;
}

static int instrumentor_indirect_ops_get_orig_operations_nodata(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void** ops_orig)
{
    /*
     * Scenarios of getting such situation:
     * 
     * 1. object2->ops = object1->ops,
     * 
     * where object1 is watched by us, but object2 is not.
     * 
     * 2. object2->ops->op1 = object1->ops->op1,
     * 
     * where object1 is watched by us, but object2 is not;
     * op1 is replaced operation.
     * 
     * In both cases we return original operation of object1, as one
     * which code author is probably implied when copy operation(s).
     */

    struct kedr_coi_instrumentor_indirect* instrumentor_real;
    struct instrumentor_indirect_ops_data* ops_data;
    struct kedr_coi_hash_elem* elem_tmp;
    
    const void* object_ops;
    
    instrumentor_real = instrumentor_indirect(instrumentor);
    
    object_ops = instrumentor_indirect_object_ops_get(instrumentor_real,
        object);

    elem_tmp = kedr_coi_hash_table_find_elem(
        &instrumentor_real->hash_table_ops_repl,
        object_ops);
    
    if(elem_tmp == NULL)
    {
        /*
         * There are 2 scenarios for get this situation:
         * 
         * 1) object's operations changed from ops_repl for some object
         * to its original operations just after the intermediate function call.
         * 2) we forgot object, from wich operation was copied to another object.
         * 
         * Second scenario is unrecoverable, so it is better to return error.
         * 
         * In the first scenario we should return current object operations,
         * but this lead to infinitive recursion in the scenario 2, that
         * is worse than BUG() for the system.
         */
        
        return -ENOENT;
    }
    
    ops_data = container_of(elem_tmp, typeof(*ops_data), hash_elem_ops_repl);
    
    *ops_orig = ops_data->ops_orig;
    
    return 0;
}

static void
instrumentor_indirect_ops_free_instance(
    struct kedr_coi_instrumentor* instrumentor)
{
    struct kedr_coi_instrumentor_indirect* instrumentor_real =
        instrumentor_indirect(instrumentor);
    
    kedr_coi_hash_table_destroy(&instrumentor_real->hash_table_ops_orig, NULL);
    kedr_coi_hash_table_destroy(&instrumentor_real->hash_table_ops_repl, NULL);
    
    kfree(instrumentor_real);
}

static struct kedr_coi_instrumentor_operations instrumentor_indirect_ops =
{
    .alloc_object_data = instrumentor_indirect_ops_alloc_object_data,
    .free_object_data = instrumentor_indirect_ops_free_object_data,
    
    .replace_operations = instrumentor_indirect_ops_replace_operations,
    .restore_operations = instrumentor_indirect_ops_restore_operations,
    .update_operations = instrumentor_indirect_ops_update_operations,
    
    .get_orig_operations = instrumentor_indirect_ops_get_orig_operations,
    .get_orig_operations_nodata = instrumentor_indirect_ops_get_orig_operations_nodata,
    
    .free_instance = instrumentor_indirect_ops_free_instance
};

//*************Empty variant of indirect instrumentor*****************
struct kedr_coi_instrumentor_indirect_empty
{
    struct kedr_coi_instrumentor_empty base;
    size_t operations_field_offset;
};

#define instrumentor_indirect_empty(instrumentor) container_of(instrumentor, struct kedr_coi_instrumentor_indirect_empty, base)

static const void*
instrumentor_indirect_empty_ops_get_object_global_key(
    struct kedr_coi_instrumentor_empty* instrumentor,
    const void* object)
{
    struct kedr_coi_instrumentor_indirect_empty* instrumentor_real =
        instrumentor_indirect_empty(instrumentor);
    
    return (const char*)object + instrumentor_real->operations_field_offset;
}

static void
instrumentor_indirect_empty_ops_free_instance(
    struct kedr_coi_instrumentor_empty* instrumentor)
{
    kfree(instrumentor_indirect_empty(instrumentor));
}

static struct instrumentor_empty_operations instrumentor_indirect_empty_ops =
{
    .get_object_global_key = instrumentor_indirect_empty_ops_get_object_global_key,
    .free_instance = instrumentor_indirect_empty_ops_free_instance
};

// Constructor for indirect instrumentor
struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_indirect(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result;

    struct kedr_coi_instrumentor_indirect* instrumentor;
    
    if((replacements == NULL) || (replacements[0].operation_offset == -1))
    {
        struct kedr_coi_instrumentor_indirect_empty* instrumentor_empty =
            kmalloc(sizeof(*instrumentor_empty), GFP_KERNEL);
        
        if(instrumentor_empty == NULL) return NULL;
        if(instrumentor_empty_init(&instrumentor_empty->base,
            &instrumentor_indirect_empty_ops))
        {
            kfree(instrumentor_empty);
            return NULL;
        }
        
        instrumentor_empty->operations_field_offset = operations_field_offset;
        
        return &instrumentor_empty->base.base;
    }
    
    instrumentor = kmalloc(sizeof(*instrumentor), GFP_KERNEL);
    
    if(instrumentor == NULL) return NULL;
    
    result = kedr_coi_hash_table_init(&instrumentor->hash_table_ops_orig);
    if(result) goto hash_table_ops_orig_err;

    result = kedr_coi_hash_table_init(&instrumentor->hash_table_ops_repl);
    if(result) goto hash_table_ops_repl_err;

    result = instrumentor_common_init(&instrumentor->base, 
        &instrumentor_indirect_ops);
    if(result) goto instrumentor_common_init_err;
    
    instrumentor->operations_field_offset = operations_field_offset;
    instrumentor->operations_struct_size = operations_struct_size;
    instrumentor->replacements = replacements;
    
    return &instrumentor->base;

instrumentor_common_init_err:
    kedr_coi_hash_table_destroy(&instrumentor->hash_table_ops_repl, NULL);
hash_table_ops_repl_err:
    kedr_coi_hash_table_destroy(&instrumentor->hash_table_ops_orig, NULL);
hash_table_ops_orig_err:
    kfree(instrumentor);
    
    return NULL;
}

//********* Instrumentor for direct operations (rarely used)************

// Per-object data of direct instrumentor.
struct instrumentor_direct_object_data
{
    struct kedr_coi_instrumentor_object_data base;
    // Object with filled original operations(other fields are uninitialized).
    void* object_ops_orig;
};

#define object_data_direct(object_data) container_of(object_data, struct instrumentor_direct_object_data, base)

// Object type of direct instrumentor.
struct kedr_coi_instrumentor_direct
{
    struct kedr_coi_instrumentor base;
    
    size_t object_struct_size;
    // NOTE: this field is not NULL(without replacements empty variant of instrumentor is created)
    const struct kedr_coi_replacement* replacements;
};

// Convert pointer to the common instrumentor to the pointer to direct one.
#define instrumentor_direct(instrumentor) container_of(instrumentor, struct kedr_coi_instrumentor_direct, base)

// Return global key for object
static inline const void*
instrumentor_direct_object_key_global(
    struct kedr_coi_instrumentor_direct* instrumentor,
    const void* object)
{
    return object;
}

static inline void**
instrumentor_direct_object_op_p(void* object,
    size_t operation_offset)
{
    return (void**)((char*)object + operation_offset);
}
//************ Operations of direct instrumentor ***********************
static struct kedr_coi_instrumentor_object_data*
instrumentor_direct_ops_alloc_object_data(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object)
{
    struct kedr_coi_instrumentor_direct* instrumentor_real;
    struct instrumentor_direct_object_data* object_data;
    
    instrumentor_real = instrumentor_direct(instrumentor);
    // TODO: kmem_cache may be more appropriate
    object_data = kmalloc(sizeof(*object_data), GFP_KERNEL);
    if(object_data == NULL) return NULL;
    
    // TODO: kmem_cache may be more appropriate
    object_data->object_ops_orig = kmalloc(
        instrumentor_real->object_struct_size, GFP_KERNEL);
    if(object_data->object_ops_orig == NULL)
    {
        kfree(object_data);
        return NULL;
    }
    
    instrumentor_object_data_init(&object_data->base,
        object,
        instrumentor_direct_object_key_global(instrumentor_real, object));
    
    return &object_data->base;
}

static void
instrumentor_direct_ops_free_object_data(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    struct instrumentor_direct_object_data* object_data_real =
        object_data_direct(object_data);
    
    kfree(object_data_real->object_ops_orig);
    kfree(object_data_real);
}

static int
instrumentor_direct_ops_replace_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct kedr_coi_instrumentor_direct* instrumentor_real;
    struct instrumentor_direct_object_data* object_data_real;
    const struct kedr_coi_replacement* replacement;
    char* object_ops_orig;
    
    instrumentor_real = instrumentor_direct(instrumentor);
    object_data_real = object_data_direct(object_data);
    object_ops_orig = object_data_real->object_ops_orig;

    for(replacement = instrumentor_real->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        size_t operation_offset = replacement->operation_offset;
        void** operation_p = instrumentor_direct_object_op_p(
            object, operation_offset);
         
        *instrumentor_direct_object_op_p(
            object_ops_orig, operation_offset) = *operation_p;
        
        *operation_p = replacement->repl;
    }
    
    return 0;
}

static void
instrumentor_direct_ops_restore_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct kedr_coi_instrumentor_direct* instrumentor_real;
    struct instrumentor_direct_object_data* object_data_real;
    const struct kedr_coi_replacement* replacement;
    char* object_ops_orig;

    instrumentor_real = instrumentor_direct(instrumentor);
    object_data_real = object_data_direct(object_data);
    object_ops_orig = object_data_real->object_ops_orig;
        
    for(replacement = instrumentor_real->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        size_t operation_offset = replacement->operation_offset;
        void** operation_p = instrumentor_direct_object_op_p(
            object, operation_offset);
         
        if(*operation_p == replacement->repl)
            *operation_p = *instrumentor_direct_object_op_p(
                object_ops_orig, operation_offset);
    }
}


static int
instrumentor_direct_ops_update_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct kedr_coi_instrumentor_direct* instrumentor_real;
    struct instrumentor_direct_object_data* object_data_real;
    const struct kedr_coi_replacement* replacement;
    char* object_ops_orig;

    instrumentor_real = instrumentor_direct(instrumentor);
    object_data_real = object_data_direct(object_data);
    object_ops_orig = object_data_real->object_ops_orig;
        
    for(replacement = instrumentor_real->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        size_t operation_offset = replacement->operation_offset;
        void** operation_p = instrumentor_direct_object_op_p(
            object, operation_offset);
         
        if(*operation_p != replacement->repl)
        {
            *instrumentor_direct_object_op_p(
                object_ops_orig, operation_offset) = *operation_p;
            
            *operation_p = replacement->repl;
        }
    }
    
    return 0;
}

static int
instrumentor_direct_ops_get_orig_operations(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_object_data* object_data,
    const void** ops_orig)
{
    struct instrumentor_direct_object_data* object_data_real =
        object_data_direct(object_data);
    
    *ops_orig = object_data_real->object_ops_orig;
    
    return 0;
}

static void
instrumentor_direct_ops_free_instance(
    struct kedr_coi_instrumentor* instrumentor)
{
    struct kedr_coi_instrumentor_direct* instrumentor_real =
        instrumentor_direct(instrumentor);
    
    kfree(instrumentor_real);
}

struct kedr_coi_instrumentor_operations instrumentor_direct_ops =
{
    .alloc_object_data = instrumentor_direct_ops_alloc_object_data,
    .free_object_data = instrumentor_direct_ops_free_object_data,
    
    .replace_operations = instrumentor_direct_ops_replace_operations,
    .restore_operations = instrumentor_direct_ops_restore_operations,
    .update_operations = instrumentor_direct_ops_update_operations,
    
    .get_orig_operations = instrumentor_direct_ops_get_orig_operations,
    
    .free_instance = instrumentor_direct_ops_free_instance
};

//*************Empty variant of direct instrumentor*****************

static const void*
instrumentor_direct_empty_ops_get_object_global_key(
    struct kedr_coi_instrumentor_empty* instrumentor,
    const void* object)
{
    return object;
}

static void
instrumentor_direct_empty_ops_free_instance(
    struct kedr_coi_instrumentor_empty* instrumentor)
{
    kfree(instrumentor);
}

static struct instrumentor_empty_operations instrumentor_direct_empty_ops =
{
    .get_object_global_key = instrumentor_direct_empty_ops_get_object_global_key,
    .free_instance = instrumentor_direct_empty_ops_free_instance
};


//Constructor
struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_direct(
    size_t object_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_instrumentor_direct* instrumentor;
    
    if(replacements == NULL || replacements[0].operation_offset == -1)
    {
        struct kedr_coi_instrumentor_empty* instrumentor_empty =
            kmalloc(sizeof(*instrumentor_empty), GFP_KERNEL);
        
        if(instrumentor_empty == NULL) return NULL;
        
        if(instrumentor_empty_init(instrumentor_empty,
            &instrumentor_direct_empty_ops))
        {
            kfree(instrumentor_empty);
            return NULL;
        }
        
        return &instrumentor_empty->base;
    }
    
    instrumentor = kmalloc(sizeof(*instrumentor), GFP_KERNEL);
    
    if(instrumentor == NULL) return NULL;
    
    if(instrumentor_common_init(&instrumentor->base,
        &instrumentor_direct_ops))
    {
        kfree(instrumentor);
        return NULL;
    }
    
    instrumentor->object_struct_size = object_struct_size;
    instrumentor->replacements = replacements;
    
    return &instrumentor->base;
}

//*******Initialize and destroy instrumentors support*****************
int kedr_coi_instrumentors_init(void)
{
    return kedr_coi_global_map_init();
}

void kedr_coi_instrumentors_destroy(void)
{
    kedr_coi_global_map_destroy();
}


//**************** Implementation of the global map of objects ********

static DEFINE_SPINLOCK(global_map_lock);

static struct kedr_coi_hash_table global_map;

void kedr_coi_global_key_init(
    struct kedr_coi_global_key* key,
    const void* value)
{
    kedr_coi_hash_elem_init(&key->hash_elem, value);
}

int kedr_coi_global_map_init(void)
{
    return kedr_coi_hash_table_init(&global_map);
}

static void global_map_free_elem(struct kedr_coi_hash_elem* elem,
    struct kedr_coi_hash_table* table)
{
    pr_warning("Element with key %p wasn't removed from global map.",
        elem->key);
}
void kedr_coi_global_map_destroy(void)
{
    return kedr_coi_hash_table_destroy(&global_map, global_map_free_elem);
}


int kedr_coi_global_map_add_key_unique(struct kedr_coi_global_key* key)
{
    int result;
    unsigned long flags;
    spin_lock_irqsave(&global_map_lock, flags);
    
    if(kedr_coi_hash_table_find_elem(&global_map, key->hash_elem.key) == NULL)
    {
        result = kedr_coi_hash_table_add_elem(&global_map, &key->hash_elem);
    }
    else
    {
        result = -EBUSY;
    }
    
    spin_unlock_irqrestore(&global_map_lock, flags);
    
    return result;
}

void kedr_coi_global_map_remove_key(struct kedr_coi_global_key* key)
{
    unsigned long flags;
    spin_lock_irqsave(&global_map_lock, flags);

    kedr_coi_hash_table_remove_elem(&global_map, &key->hash_elem);

    spin_unlock_irqrestore(&global_map_lock, flags);
}
