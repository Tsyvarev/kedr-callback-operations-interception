/*
 * Implementation of instrumentors of concrete types.
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

static inline void*
get_operation_at_offset(const void* ops, size_t operation_offset)
{
    return *((void**)((const char*)ops + operation_offset));
}


//*********** Instrumentor which does not instrument at all*************

/*
 * Such instrumentor is a fallback for instrumentor's constructors
 * when nothing to replace.
 */

static struct kedr_coi_instrumentor_object_data*
instrumentor_empty_i_iface_alloc_object_data(
    struct kedr_coi_object* i_iface_impl)
{
    return kmalloc(sizeof(struct kedr_coi_instrumentor_object_data), GFP_KERNEL);
}

static void instrumentor_empty_i_iface_free_object_data(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    kfree(object_data);
}

static int instrumentor_empty_i_iface_replace_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object)
{
    return 0;// nothing to replace
}

static void instrumentor_empty_i_iface_clean_replacement(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object,
    int norestore)
{
}


static int instrumentor_empty_i_iface_update_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object)
{
    return 0;// up-to-date
}

//********* Instrumentor for indirect operations (mostly used)*********/

/*
 * Global map for prevent different indirect instrumentors to perform
 * operations replacements which has undesireable effect on each other.
 * 
 * In this concrete case, map contains pointers to operations fields.
 */
static int indirect_operations_fields_add_unique(
    struct kedr_coi_hash_elem* operations_field_elem);
static void indirect_operations_fields_remove(
    struct kedr_coi_hash_elem* operations_field_elem);


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

    struct kedr_coi_hash_elem operations_field_elem;
    // Pointer to the operations data, which is referenced by this object
    struct instrumentor_indirect_ops_data* ops_data;
};

struct instrumentor_indirect_impl
{
    struct kedr_coi_object base;
    // hash table with replaced operations as keys
    struct kedr_coi_hash_table hash_table_ops_repl;
    // hash table with original operations as keys
    struct kedr_coi_hash_table hash_table_ops_orig;
    // protect tables with operations data from concurrent access
    spinlock_t ops_lock;
    
    size_t operations_struct_size;
    size_t operations_field_offset;
    // NOTE: this field is not NULL(without replacements empty variant of instrumentor is created)
    const struct kedr_coi_replacement* replacements;
};


// Return pointer to operations struct for the given object.
static inline const void**
instrumentor_indirect_impl_object_ops_p_get(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* object)
{
    void* operations_field =
        (char*) object + instrumentor_impl->operations_field_offset;
    return (const void**)operations_field;
}

/*
 * Allocate instance of per-operations data and fill its some fields.
 * 
 * NOTE: May be called in atomic context.
 */
static struct instrumentor_indirect_ops_data*
instrumentor_indirect_impl_alloc_ops_data(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* ops_orig)
{
    void* ops_repl;
    struct instrumentor_indirect_ops_data* ops_data =
        kmalloc(sizeof(*ops_data), GFP_ATOMIC);
    
    if(ops_data == NULL) return NULL;
    
    ops_repl = kmalloc(instrumentor_impl->operations_struct_size, GFP_ATOMIC);
    
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
instrumentor_indirect_impl_free_ops_data(
    struct instrumentor_indirect_impl* instrumentor_impl,
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
instrumentor_indirect_impl_fill_ops_data(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_ops_data* ops_data,
    const void* ops_orig)
{
    const struct kedr_coi_replacement* replacement;
    char* ops_repl = ops_data->ops_repl;

    ops_data->ops_orig = ops_orig;
    
    memcpy(ops_repl, ops_orig,
        instrumentor_impl->operations_struct_size);
    
    for(replacement = instrumentor_impl->replacements;
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
 * 
 * Should be executed under lock.
 */
static void
instrumentor_indirect_impl_unref_ops_data(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_ops_data* ops_data)
{
    if(--ops_data->refs != 0) return;
    
    kedr_coi_hash_table_remove_elem(&instrumentor_impl->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig);
    
    kedr_coi_hash_table_remove_elem(&instrumentor_impl->hash_table_ops_repl,
        &ops_data->hash_elem_ops_repl);

    instrumentor_indirect_impl_free_ops_data(instrumentor_impl, ops_data);
}


/* 
 * Increment refcount of the instance of per-operations data.
 * 
 * Should be executed under lock.
 */
static void
instrumentor_indirect_impl_ref_ops_data(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_ops_data* ops_data)
{
    ops_data->refs ++;
}

/*
 * Return per-operations data which corresponds to the given operations.
 * If such data not found, return NULL.
 * 
 * Should be executed under lock.
 */
static struct instrumentor_indirect_ops_data*
instrumentor_indirect_impl_find_ops_data(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* object_ops)
{
    struct instrumentor_indirect_ops_data* ops_data;
    struct kedr_coi_hash_elem* elem_tmp;
    
    // Look for existed per-operations data for that operations
    elem_tmp = kedr_coi_hash_table_find_elem(
        &instrumentor_impl->hash_table_ops_orig,
        object_ops);
    if(elem_tmp != NULL)
    {
        ops_data = container_of(elem_tmp, typeof(*ops_data), hash_elem_ops_orig);
        return ops_data;
    }
    
    elem_tmp = kedr_coi_hash_table_find_elem(
        &instrumentor_impl->hash_table_ops_repl,
        object_ops);

    if(elem_tmp != NULL)
    {
        ops_data = container_of(elem_tmp, typeof(*ops_data), hash_elem_ops_repl);
        return ops_data;
    }

    return NULL;
}

/*
 * Return per-operations data which corresponds to the given operations
 * and 'ref' these data.
 * 
 * If such data don't exist, create new one and return it.
 * 
 * Should be executed under lock.
 */

static struct instrumentor_indirect_ops_data*
instrumentor_indirect_impl_get_ops_data(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* object_ops)
{
    struct instrumentor_indirect_ops_data* ops_data;
    
    // Look for existed per-operations data for that operations
    ops_data = instrumentor_indirect_impl_find_ops_data(
        instrumentor_impl,
        object_ops);
    
    if(ops_data)
    {
        instrumentor_indirect_impl_ref_ops_data(instrumentor_impl, ops_data);
        return ops_data;
    }

    // Per-operations data for these operations are not exist, create new one
    ops_data = instrumentor_indirect_impl_alloc_ops_data(instrumentor_impl,
        object_ops);

    if(ops_data == NULL) return NULL;
    
    instrumentor_indirect_impl_fill_ops_data(instrumentor_impl, ops_data,
        object_ops);
    
    if(kedr_coi_hash_table_add_elem(
        &instrumentor_impl->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig))
    {
        instrumentor_indirect_impl_free_ops_data(instrumentor_impl, ops_data);
        return NULL;
    }
    
    if(kedr_coi_hash_table_add_elem(
        &instrumentor_impl->hash_table_ops_repl,
        &ops_data->hash_elem_ops_repl))
    {
        kedr_coi_hash_table_remove_elem(
            &instrumentor_impl->hash_table_ops_orig,
            &ops_data->hash_elem_ops_orig);
        instrumentor_indirect_impl_free_ops_data(instrumentor_impl, ops_data);
        return NULL;
    }

    return ops_data;
}

//*** Low level API for implementing interface for indirect instrumentor***

static int instrumentor_indirect_impl_init(
    struct instrumentor_indirect_impl* instrumentor_impl,
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result;

    BUG_ON(replacements == NULL);
    
    result = kedr_coi_hash_table_init(&instrumentor_impl->hash_table_ops_orig);
    if(result) goto hash_table_ops_orig_err;

    result = kedr_coi_hash_table_init(&instrumentor_impl->hash_table_ops_repl);
    if(result) goto hash_table_ops_repl_err;

    instrumentor_impl->operations_field_offset = operations_field_offset;
    instrumentor_impl->operations_struct_size = operations_struct_size;
    instrumentor_impl->replacements = replacements;
    
    spin_lock_init(&instrumentor_impl->ops_lock);
    
    return 0;

hash_table_ops_repl_err:
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_orig, NULL);
hash_table_ops_orig_err:
    return result;
}

static void
instrumentor_indirect_impl_destroy(
    struct instrumentor_indirect_impl* instrumentor_impl)
{
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_repl, NULL);
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_orig, NULL);
}


const void*
instrumentor_indirect_object_data_get_repl_operations(
    struct instrumentor_indirect_object_data* object_data)
{
    return object_data->ops_data->ops_repl;
}

const void*
instrumentor_indirect_object_data_get_orig_operations(
    struct instrumentor_indirect_object_data* object_data)
{
    return object_data->ops_data->ops_orig;
}


static int
instrumentor_indirect_impl_replace_operations(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_object_data* object_data_new,
    void* object)
{
    unsigned long flags;
    struct instrumentor_indirect_ops_data* ops_data;
    
    int result;
    const void** object_ops_p = 
        instrumentor_indirect_impl_object_ops_p_get(instrumentor_impl,
            object);

    /* 
     * Check for collisions with another instrumentors.
     */

    kedr_coi_hash_elem_init(
        &object_data_new->operations_field_elem,
        object_ops_p);

    result = indirect_operations_fields_add_unique(
        &object_data_new->operations_field_elem);
    
    switch(result)
    {
    case 0:
        break;
    case -EBUSY:
        pr_err("Cannot watch for an object %p "
            "because it is already watched by another interceptor "
            "(offset of operations struct is %zu).",
            object,
            instrumentor_impl->operations_field_offset);
        return -EBUSY;
    default:
        return result;
    }

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    ops_data = instrumentor_indirect_impl_get_ops_data(instrumentor_impl,
        *object_ops_p);
    
    if(ops_data == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);

        indirect_operations_fields_remove(
            &object_data_new->operations_field_elem);
        return -ENOMEM;
    }

    object_data_new->ops_data = ops_data;

    *object_ops_p = ops_data->ops_repl;
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return 0;
}

static void instrumentor_indirect_impl_restore_ops(
    struct instrumentor_indirect_ops_data* ops_data,
    const void** ops_p)
{
    if(*ops_p == ops_data->ops_repl)
        *ops_p = ops_data->ops_orig;
}

static void
instrumentor_indirect_impl_clean_replacement(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_object_data* object_data,
    void* object,
    int norestore)
{
    unsigned long flags;
    
    if(!norestore)
    {
        instrumentor_indirect_impl_restore_ops(object_data->ops_data,
            instrumentor_indirect_impl_object_ops_p_get(
                instrumentor_impl,
                object));
    }

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    instrumentor_indirect_impl_unref_ops_data(instrumentor_impl,
        object_data->ops_data);
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    indirect_operations_fields_remove(
        &object_data->operations_field_elem);
}

static int
instrumentor_indirect_impl_update_operations(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_object_data* object_data,
    void* object)
{
    unsigned long flags;
    struct instrumentor_indirect_ops_data* ops_data;
    struct instrumentor_indirect_ops_data* ops_data_new;
    
    const void** object_ops_p;
    
    object_ops_p = instrumentor_indirect_impl_object_ops_p_get(
        instrumentor_impl,
        object);
    
    ops_data = object_data->ops_data;
    
    if(*object_ops_p == ops_data->ops_repl)
    {
        //up-to-date
        return 0;
    }
    
    if(*object_ops_p == ops_data->ops_orig)
    {
        //set replaced operations again
        *object_ops_p = ops_data->ops_repl;
        return 0;
    }
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    ops_data_new = instrumentor_indirect_impl_get_ops_data(
        instrumentor_impl,
        *object_ops_p);
    
    if(ops_data_new == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        return -ENOMEM;
    }
    
    object_data->ops_data = ops_data_new;
    
    *object_ops_p = ops_data_new->ops_repl;
    
    instrumentor_indirect_impl_unref_ops_data(instrumentor_impl, ops_data);

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return 0;
}

static int instrumentor_indirect_impl_get_orig_operations_nodata(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* object_ops,
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

    unsigned long flags;
    struct instrumentor_indirect_ops_data* ops_data;
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    ops_data = instrumentor_indirect_impl_find_ops_data(
        instrumentor_impl,
        object_ops);
    
    if(ops_data == NULL)
    {
        /*
         * There are 2 scenarios for get this situation:
         * 
         * 1) object's operations changed from ops_repl for some object
         * to its original operations just after the intermediate function call.
         * 2) we forgot object, from which operations was copied to another object.
         * 
         * Second scenario is unrecoverable, so it is better to return error.
         * 
         * In the first scenario we should return current object operations,
         * but this lead to infinitive recursion in the scenario 2, that
         * is worse than BUG() for the system.
         */
        
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        return -ENOENT;
    }
    
    *ops_orig = ops_data->ops_orig;

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return 0;
}


//*************Interface of the indirect instrumentor****************

static struct kedr_coi_instrumentor_object_data*
instrumentor_indirect_impl_i_iface_alloc_object_data(
    struct kedr_coi_object* i_iface_impl)
{
    struct instrumentor_indirect_object_data* object_data =
        kmalloc(sizeof(*object_data), GFP_KERNEL);

    if(object_data == NULL) return NULL;
    
    return &object_data->base;
}

static void
instrumentor_indirect_impl_i_iface_free_object_data(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    struct instrumentor_indirect_object_data* object_data_real;

    object_data_real = container_of(object_data, struct instrumentor_indirect_object_data, base);
    
    kfree(object_data_real);
}


static int
instrumentor_indirect_impl_i_iface_replace_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object)
{
    struct instrumentor_indirect_impl* instrumentor_impl;
    struct instrumentor_indirect_object_data* object_data_new_real;
    
    instrumentor_impl = container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    object_data_new_real = container_of(object_data_new, typeof(*object_data_new_real), base);
    
    return instrumentor_indirect_impl_replace_operations(
        instrumentor_impl,
        object_data_new_real,
        object);
}

static void
instrumentor_indirect_impl_i_iface_clean_replacement(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object,
    int norestore)
{
    struct instrumentor_indirect_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    struct instrumentor_indirect_impl* instrumentor_impl = 
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_indirect_impl_clean_replacement(
        instrumentor_impl,
        object_data_real,
        object,
        norestore);
}

static int
instrumentor_indirect_impl_i_iface_update_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct instrumentor_indirect_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    struct instrumentor_indirect_impl* instrumentor_impl = 
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_indirect_impl_update_operations(
        instrumentor_impl,
        object_data_real,
        object);
}

//********* Instrumentor for direct operations (rarely used)************

/*
 * Global map for prevent different direct instrumentors to perform
 * operations replacements which has undesireable effect on each other.
 * 
 * In this concrete case, map contains pointers to objects.
 */
static int objects_directly_instrumented_add_unique(
    struct kedr_coi_hash_elem* object_elem);

static void objects_directly_instrumented_remove(
    struct kedr_coi_hash_elem* object_elem);

// Per-object data of direct instrumentor.
struct instrumentor_direct_object_data
{
    struct kedr_coi_instrumentor_object_data base;

    struct kedr_coi_hash_elem object_elem;
    // Object with filled original operations(other fields are uninitialized).
    void* object_ops_orig;
};

// Object type of direct instrumentor.
struct instrumentor_direct_impl
{
    struct kedr_coi_object base;
    size_t object_struct_size;
    // NOTE: this field is not NULL(without replacements empty variant of instrumentor is created)
    const struct kedr_coi_replacement* replacements;
};


static inline void**
instrumentor_direct_impl_object_op_p(void* object,
    size_t operation_offset)
{
    return (void**)((char*)object + operation_offset);
}
//*****Low level API of the direct instrumentor implementation **********
static int
instrumentor_direct_impl_init_object_data(
    struct instrumentor_direct_impl* instrumentor,
    struct instrumentor_direct_object_data* object_data)
{
    // TODO: kmem_cache may be more appropriate
    object_data->object_ops_orig = kmalloc(
        instrumentor->object_struct_size, GFP_KERNEL);
    if(object_data->object_ops_orig == NULL)
    {
        return -ENOMEM;
    }
    
    return 0;
}

static void
instrumentor_direct_impl_destroy_object_data(
    struct instrumentor_direct_impl* instrumentor,
    struct instrumentor_direct_object_data* object_data)
{
    kfree(object_data->object_ops_orig);
}

static int
instrumentor_direct_impl_init(
    struct instrumentor_direct_impl* instrumentor,
    size_t object_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    BUG_ON(replacements == NULL);
    
    instrumentor->object_struct_size = object_struct_size;
    instrumentor->replacements = replacements;

    return 0;
}


static void
instrumentor_direct_impl_destroy(
    struct instrumentor_direct_impl* instrumentor)
{
}


static int
instrumentor_direct_impl_replace_operations(
    struct instrumentor_direct_impl* instrumentor_impl,
    struct instrumentor_direct_object_data* object_data,
    void* object)
{
    const struct kedr_coi_replacement* replacement;
    char* object_ops_orig;
    
    // Check collisions with other instrumentors.
    int result;
    kedr_coi_hash_elem_init(&object_data->object_elem, object);
    
    result = objects_directly_instrumented_add_unique(
        &object_data->object_elem);

    switch(result)
    {
    case 0:
        break;
    case -EBUSY:
        pr_err("Cannot watch for an object %p because it is already "
            "watched by another interceptor.", object);
        return -EBUSY;
    default:
        return result;
    }

    object_ops_orig = object_data->object_ops_orig;

    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        size_t operation_offset = replacement->operation_offset;
        void** operation_p = instrumentor_direct_impl_object_op_p(
            object, operation_offset);
         
        *instrumentor_direct_impl_object_op_p(
            object_ops_orig, operation_offset) = *operation_p;
        
        *operation_p = replacement->repl;
    }
    
    return 0;
}

static void
instrumentor_direct_impl_clean_replacement(
    struct instrumentor_direct_impl* instrumentor_impl,
    struct instrumentor_direct_object_data* object_data,
    void* object,
    int norestore)
{
    const struct kedr_coi_replacement* replacement;
    char* object_ops_orig;
    
    object_ops_orig = object_data->object_ops_orig;
        
    if(!norestore)
    {
        for(replacement = instrumentor_impl->replacements;
            replacement->operation_offset != -1;
            replacement++)
        {
            size_t operation_offset = replacement->operation_offset;
            void** operation_p = instrumentor_direct_impl_object_op_p(
                object, operation_offset);
             
            if(*operation_p == replacement->repl)
                *operation_p = *instrumentor_direct_impl_object_op_p(
                    object_ops_orig, operation_offset);
        }
    }
    
    objects_directly_instrumented_remove(&object_data->object_elem);
}


static int
instrumentor_direct_impl_update_operations(
    struct instrumentor_direct_impl* instrumentor_impl,
    struct instrumentor_direct_object_data* object_data,
    void* object)
{
    const struct kedr_coi_replacement* replacement;
    char* object_ops_orig;

    object_ops_orig = object_data->object_ops_orig;
        
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        size_t operation_offset = replacement->operation_offset;
        void** operation_p = instrumentor_direct_impl_object_op_p(
            object, operation_offset);
         
        if(*operation_p != replacement->repl)
        {
            *instrumentor_direct_impl_object_op_p(
                object_ops_orig, operation_offset) = *operation_p;
            
            *operation_p = replacement->repl;
        }
    }
    
    return 0;
}

static int
instrumentor_direct_impl_get_orig_operation(
    struct instrumentor_direct_impl* instrumentor_impl,
    struct instrumentor_direct_object_data* object_data,
    size_t operation_offset,
    void** op_orig)
{
    *op_orig = get_operation_at_offset(object_data->object_ops_orig,
        operation_offset);
    
    return 0;
}

//************ Interface of direct instrumentor ***********************
static struct kedr_coi_instrumentor_object_data*
instrumentor_direct_impl_i_iface_alloc_object_data(
    struct kedr_coi_object* i_iface_impl)
{
    int result;
    
    struct instrumentor_direct_object_data* object_data;
    
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    object_data = kmalloc(sizeof(*object_data), GFP_KERNEL);
    if(object_data == NULL) return NULL;
    
    result = instrumentor_direct_impl_init_object_data(
        instrumentor_impl,
        object_data);
    
    if(result)
    {
        kfree(object_data);
        return NULL;
    }
    
    return &object_data->base;
}

static void
instrumentor_direct_impl_i_iface_free_object_data(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    struct instrumentor_direct_object_data* object_data_real;
    
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    object_data_real = container_of(object_data, typeof(*object_data_real), base);
    
    instrumentor_direct_impl_destroy_object_data(
        instrumentor_impl,
        object_data_real);

    kfree(object_data_real);
}

static int
instrumentor_direct_impl_i_iface_replace_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct instrumentor_direct_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);

    return instrumentor_direct_impl_replace_operations(
        instrumentor_impl,
        object_data_real,
        object);
}

static void
instrumentor_direct_impl_i_iface_clean_replacement(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object,
    int norestore)
{
    struct instrumentor_direct_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_direct_impl_clean_replacement(
        instrumentor_impl,
        object_data_real,
        object,
        norestore);
}


static int
instrumentor_direct_impl_i_iface_update_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct instrumentor_direct_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_direct_impl_update_operations(
        instrumentor_impl,
        object_data_real,
        object);
}

static void instrumentor_empty_i_iface_destroy_impl(
    struct kedr_coi_object* i_iface_impl)
{
    kfree(i_iface_impl);
}

//*********** Creation of the empty instrumentor as normal**********************
struct kedr_coi_instrumentor_normal_impl_iface instrumentor_empty_in_iface =
{
    .i_iface =
    {
        .alloc_object_data = instrumentor_empty_i_iface_alloc_object_data,
        .free_object_data = instrumentor_empty_i_iface_free_object_data,
        
        .replace_operations = instrumentor_empty_i_iface_replace_operations,
        .clean_replacement = instrumentor_empty_i_iface_clean_replacement,
        .update_operations = instrumentor_empty_i_iface_update_operations,
        .destroy_impl = instrumentor_empty_i_iface_destroy_impl
    }
    // Other fields should not be accessed
};

static struct kedr_coi_instrumentor_normal*
instrumentor_empty_create(void)
{
    struct kedr_coi_instrumentor_normal* instrumentor;
    struct kedr_coi_object* instrumentor_empty_impl = 
        kmalloc(sizeof(*instrumentor_empty_impl), GFP_KERNEL);
    
    if(instrumentor_empty_impl == NULL)
    {
        pr_err("Failed to allocate structure for empty instrumentor.");
        return NULL;
    }
    
    instrumentor_empty_impl->interface = &instrumentor_empty_in_iface.i_iface.base;

    instrumentor = kedr_coi_instrumentor_normal_create(instrumentor_empty_impl);
    if(instrumentor == NULL)
    {
        kfree(instrumentor_empty_impl);
        return NULL;
    }
    
    return instrumentor;
}

//******** Indirect instrumentor for normal operations *************

//********Interface of the indirect instrumentor as normal*************
static int instrumentor_indirect_impl_in_iface_get_orig_operation(
    struct kedr_coi_object* in_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    const void* object,
    size_t operation_offset,
    void** op_orig)
{
    struct instrumentor_indirect_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    *op_orig = get_operation_at_offset(
        object_data_real->ops_data->ops_orig,
        operation_offset);
    
    return 0;
}

static int instrumentor_indirect_impl_in_iface_get_orig_operation_nodata(
    struct kedr_coi_object* in_iface_impl,
    const void* object,
    size_t operation_offset,
    void** op_orig)
{
    int result;
    const void* ops_orig;

    struct instrumentor_indirect_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);
    
    result = instrumentor_indirect_impl_get_orig_operations_nodata(
        instrumentor_impl,
        *instrumentor_indirect_impl_object_ops_p_get(instrumentor_impl, object),
        &ops_orig);
    
    if(result) return result;

    *op_orig = get_operation_at_offset(ops_orig, operation_offset);
    
    return 0;
}

static void instrumentor_indirect_impl_in_iface_destroy_impl(
    struct kedr_coi_object* i_iface_impl)
{
    struct instrumentor_indirect_impl* impl_real =
        container_of(i_iface_impl, typeof(*impl_real), base);
    
    instrumentor_indirect_impl_destroy(impl_real);
    
    kfree(impl_real);
}

static struct kedr_coi_instrumentor_normal_impl_iface instrumentor_indirect_impl_in_iface =
{
    .i_iface =
    {
        .alloc_object_data = instrumentor_indirect_impl_i_iface_alloc_object_data,
        .free_object_data = instrumentor_indirect_impl_i_iface_free_object_data,

        .replace_operations = instrumentor_indirect_impl_i_iface_replace_operations,
        .clean_replacement = instrumentor_indirect_impl_i_iface_clean_replacement,
        .update_operations = instrumentor_indirect_impl_i_iface_update_operations,

        .destroy_impl = instrumentor_indirect_impl_in_iface_destroy_impl
    },
    
    .get_orig_operation = instrumentor_indirect_impl_in_iface_get_orig_operation,
    .get_orig_operation_nodata = instrumentor_indirect_impl_in_iface_get_orig_operation_nodata
};


// Constructor for indirect instrumentor
struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_create_indirect(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_instrumentor_normal* instrumentor;
    
    int result;

    struct instrumentor_indirect_impl* instrumentor_impl;
    
    if((replacements == NULL) || (replacements[0].operation_offset == -1))
    {
        return instrumentor_empty_create();
    }
    
    instrumentor_impl = kmalloc(sizeof(*instrumentor_impl), GFP_KERNEL);
    
    if(instrumentor_impl == NULL)
    {
        pr_err("Failed to allocate structure for indirect instrumentor implementation");
        return NULL;
    }
    
    result = instrumentor_indirect_impl_init(instrumentor_impl,
        operations_field_offset,
        operations_struct_size,
        replacements);
    if(result)
    {
        kfree(instrumentor_impl);
        return NULL;
    }

    instrumentor_impl->base.interface = &instrumentor_indirect_impl_in_iface.i_iface.base;
    
    instrumentor = kedr_coi_instrumentor_normal_create(&instrumentor_impl->base);
    
    if(instrumentor == NULL)
    {
        instrumentor_indirect_impl_destroy(instrumentor_impl);
        kfree(instrumentor_impl);
        return NULL;
    }

    return instrumentor;
}

//********* Direct instrumentor as normal******************

//********* Interface of direct instrumentor as normal******************
static int
instrumentor_direct_impl_in_iface_get_orig_operation(
    struct kedr_coi_object* in_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    const void* object,
    size_t operation_offset,
    void** op_orig)
{
    struct instrumentor_direct_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_direct_impl_get_orig_operation(
        instrumentor_impl,
        object_data_real,
        operation_offset,
        op_orig);
}

static int
instrumentor_direct_impl_in_iface_get_orig_operation_nodata(
    struct kedr_coi_object* in_iface_impl,
    const void* object,
    size_t operation_offset,
    void** op_orig)
{
    //Shouldn't be called
    BUG();
    
    return 0;
}


static void instrumentor_direct_impl_in_iface_destroy_impl(
    struct kedr_coi_object* i_iface_impl)
{
    struct instrumentor_direct_impl* impl_real =
        container_of(i_iface_impl, typeof(*impl_real), base);
    
    instrumentor_direct_impl_destroy(impl_real);
    
    kfree(impl_real);
}

static struct kedr_coi_instrumentor_normal_impl_iface
instrumentor_direct_impl_in_iface =
{
    .i_iface =
    {
        .alloc_object_data = instrumentor_direct_impl_i_iface_alloc_object_data,
        .free_object_data = instrumentor_direct_impl_i_iface_free_object_data,

        .replace_operations = instrumentor_direct_impl_i_iface_replace_operations,
        .clean_replacement = instrumentor_direct_impl_i_iface_clean_replacement,
        .update_operations = instrumentor_direct_impl_i_iface_update_operations,
        
        .destroy_impl = instrumentor_direct_impl_in_iface_destroy_impl
    },
    .get_orig_operation = instrumentor_direct_impl_in_iface_get_orig_operation,
    .get_orig_operation_nodata = instrumentor_direct_impl_in_iface_get_orig_operation_nodata
};


//Constructor
struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_create_direct(
    size_t object_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_instrumentor_normal* instrumentor;
    
    int result;

    struct instrumentor_direct_impl* instrumentor_impl;
    
    if((replacements == NULL) || (replacements[0].operation_offset == -1))
    {
        return instrumentor_empty_create();
    }
    
    instrumentor_impl = kmalloc(sizeof(*instrumentor_impl), GFP_KERNEL);
    
    if(instrumentor_impl == NULL)
    {
        pr_err("Failed to allocate structure for direct instrumentor implementation");
        return NULL;
    }
    
    result = instrumentor_direct_impl_init(
        instrumentor_impl,
        object_struct_size,
        replacements);
    if(result)
    {
        kfree(instrumentor_impl);
        return NULL;
    }

    instrumentor_impl->base.interface = &instrumentor_direct_impl_in_iface.i_iface.base;
    
    instrumentor = kedr_coi_instrumentor_normal_create(&instrumentor_impl->base);
    
    if(instrumentor == NULL)
    {
        instrumentor_direct_impl_destroy(instrumentor_impl);
        kfree(instrumentor_impl);
        return NULL;
    }

    return instrumentor;
}

//*********** Empty instrumentor with foreign support*****************//
static struct kedr_coi_foreign_instrumentor_impl_iface
instrumentor_empty_if_iface =
{
    .i_iface =
    {
        .alloc_object_data = instrumentor_empty_i_iface_alloc_object_data,
        .free_object_data = instrumentor_empty_i_iface_free_object_data,
        
        .replace_operations = instrumentor_empty_i_iface_replace_operations,
        .clean_replacement = instrumentor_empty_i_iface_clean_replacement,
        .update_operations = instrumentor_empty_i_iface_update_operations,
        
        .destroy_impl = instrumentor_empty_i_iface_destroy_impl
    },
    // Other fields should not be accesssed
};

static struct kedr_coi_object*
instrumentor_empty_iwf_iface_create_foreign_indirect(
    struct kedr_coi_object* iwf_iface_impl,
    size_t operations_field_offset,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_object* instrumentor_empty_impl =
        kmalloc(sizeof(*instrumentor_empty_impl), GFP_KERNEL);
    
    if(instrumentor_empty_impl == NULL)
    {
        pr_err("Failed to allocate structure for empty foreign instrumentor");
        return NULL;
    }
    
    instrumentor_empty_impl->interface = &instrumentor_empty_if_iface.i_iface.base;
    
    return instrumentor_empty_impl;
}


static struct kedr_coi_instrumentor_with_foreign_impl_iface
instrumentor_empty_iwf_iface =
{
    .in_iface =
    {
        .i_iface =
        {
            .alloc_object_data = instrumentor_empty_i_iface_alloc_object_data,
            .free_object_data = instrumentor_empty_i_iface_free_object_data,
            
            .replace_operations = instrumentor_empty_i_iface_replace_operations,
            .clean_replacement = instrumentor_empty_i_iface_clean_replacement,
            .update_operations = instrumentor_empty_i_iface_update_operations,
            
            .destroy_impl = instrumentor_empty_i_iface_destroy_impl
        },
        // Other fields should not be accesssed
    },
    .create_foreign_indirect = instrumentor_empty_iwf_iface_create_foreign_indirect,
    // Other fields should not be accesssed
};

static struct kedr_coi_instrumentor_with_foreign*
instrumentor_with_foreign_empty_create(void)
{
    struct kedr_coi_instrumentor_with_foreign* instrumentor;
    struct kedr_coi_object* instrumentor_empty_impl = 
        kmalloc(sizeof(*instrumentor_empty_impl), GFP_KERNEL);
    
    if(instrumentor_empty_impl == NULL)
    {
        pr_err("Failed to allocate structure for empty instrumentor.");
        return NULL;
    }
    
    instrumentor_empty_impl->interface = &instrumentor_empty_iwf_iface.in_iface.i_iface.base;

    instrumentor = kedr_coi_instrumentor_with_foreign_create(instrumentor_empty_impl);
    if(instrumentor == NULL)
    {
        kfree(instrumentor_empty_impl);
        return NULL;
    }
    
    return instrumentor;
}

//*********Indirect instrumentor with foreign support***************//
struct instrumentor_indirect_with_foreign_impl;

struct foreign_instrumentor_indirect_impl
{
    struct instrumentor_indirect_impl base;
    
    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl_binded;
    
    struct list_head list;
};

struct instrumentor_indirect_with_foreign_impl
{
    struct instrumentor_indirect_impl base;
    
    struct list_head foreign_instrumentor_impls;
};

//**** Low-level API of indirect instrumentor as foreign ******//
static int foreign_instrumentor_indirect_impl_init(
    struct foreign_instrumentor_indirect_impl* instrumentor_impl,
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result = instrumentor_indirect_impl_init(
        &instrumentor_impl->base,
        operations_field_offset,
        operations_struct_size,
        replacements);
    if(result) return result;
    
    INIT_LIST_HEAD(&instrumentor_impl->list);
    instrumentor_impl->instrumentor_impl_binded = NULL;

    return 0;
}

static void foreign_instrumentor_indirect_impl_destroy(
    struct foreign_instrumentor_indirect_impl* instrumentor_impl)
{
    if(!list_empty(&instrumentor_impl->list))
        list_del(&instrumentor_impl->list);
    
    instrumentor_indirect_impl_destroy(&instrumentor_impl->base);
}

//****Interface of the indirect instrumentor as foreign ****//
static int
instrumentor_indirect_impl_if_iface_restore_foreign_operations_nodata(
    struct kedr_coi_object* if_iface_impl,
    const void* prototype_object,
    void* object,
    size_t operation_offset,
    void** op_orig)
{
    int result;
    const void* ops_orig;
    
    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl_binded;
    
    struct foreign_instrumentor_indirect_impl* instrumentor_impl =
        container_of(if_iface_impl, typeof(*instrumentor_impl), base.base);
    
    instrumentor_impl_binded = instrumentor_impl->instrumentor_impl_binded;

    result = instrumentor_indirect_impl_get_orig_operations_nodata(
        &instrumentor_impl->base,
        *instrumentor_indirect_impl_object_ops_p_get(
            &instrumentor_impl_binded->base,
            object),
        &ops_orig);
    
    if(result) return result;
    
    *op_orig = get_operation_at_offset(ops_orig, operation_offset);
    
    return 0;
}

static int
instrumentor_indirect_impl_if_iface_restore_foreign_operations(
    struct kedr_coi_object* if_iface_impl,
    struct kedr_coi_instrumentor_object_data* prototype_data,
    const void* prototype_object,
    void* object,
    size_t operation_offset,
    void** op_orig)
{
    const void** object_ops_p;
    
    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl_binded;
    
    struct foreign_instrumentor_indirect_impl* instrumentor_impl =
        container_of(if_iface_impl, typeof(*instrumentor_impl), base.base);

    struct instrumentor_indirect_object_data* prototype_data_real =
        container_of(prototype_data, typeof(*prototype_data_real), base);
    
    instrumentor_impl_binded = instrumentor_impl->instrumentor_impl_binded;

    object_ops_p = instrumentor_indirect_impl_object_ops_p_get(
        &instrumentor_impl->base,
        object);
    
    if(*object_ops_p == prototype_data_real->ops_data->ops_repl)
        *object_ops_p = prototype_data_real->ops_data->ops_orig;
    
    *op_orig = get_operation_at_offset(
        prototype_data_real->ops_data->ops_orig,
        operation_offset);
    
    return 0;
}

static void instrumentor_indirect_impl_if_iface_destroy_impl(
    struct kedr_coi_object* i_iface_impl)
{
    struct foreign_instrumentor_indirect_impl* impl_real =
        container_of(i_iface_impl, typeof(*impl_real), base.base);
    
    foreign_instrumentor_indirect_impl_destroy(impl_real);
    
    kfree(impl_real);
}

static struct kedr_coi_foreign_instrumentor_impl_iface
instrumentor_indirect_impl_if_iface =
{
    //temporary changed
    //.i_iface =
    //{
        //.alloc_object_data = instrumentor_empty_i_iface_alloc_object_data,
        //.free_object_data = instrumentor_empty_i_iface_free_object_data,
        
        //.replace_operations = instrumentor_empty_i_iface_replace_operations,
        //.clean_replacement = instrumentor_empty_i_iface_clean_replacement,
        //.update_operations = instrumentor_empty_i_iface_update_operations,
        //
        //.destroy_impl = instrumentor_indirect_impl_if_iface_destroy_impl
    //},
    .i_iface =
    {
        .alloc_object_data = instrumentor_indirect_impl_i_iface_alloc_object_data,
        .free_object_data = instrumentor_indirect_impl_i_iface_free_object_data,

        .replace_operations = instrumentor_indirect_impl_i_iface_replace_operations,
        .clean_replacement = instrumentor_indirect_impl_i_iface_clean_replacement,
        .update_operations = instrumentor_indirect_impl_i_iface_update_operations,
        
        .destroy_impl = instrumentor_indirect_impl_if_iface_destroy_impl
    },
    
    .restore_foreign_operations =
        instrumentor_indirect_impl_if_iface_restore_foreign_operations,
    .restore_foreign_operations_nodata =
        instrumentor_indirect_impl_if_iface_restore_foreign_operations_nodata
};

// Constructor(for internal use) of indirect foreign instrumentor implementation
static struct foreign_instrumentor_indirect_impl*
foreign_instrumentor_indirect_impl_create_disconnected(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result;
    
    struct foreign_instrumentor_indirect_impl* instrumentor_impl =
        kmalloc(sizeof(*instrumentor_impl), GFP_KERNEL);
    
    if(instrumentor_impl == NULL)
    {
        pr_err("Failed to allocate structure for foreign instrumentor implementation");
        return NULL;
    }
    
    result = foreign_instrumentor_indirect_impl_init(
        instrumentor_impl,
        operations_field_offset,
        operations_struct_size,
        replacements);
    
    if(result)
    {
        kfree(instrumentor_impl);
        return NULL;
    }
    
    instrumentor_impl->base.base.interface = &instrumentor_indirect_impl_if_iface.i_iface.base;
    
    return instrumentor_impl;
}


//**** Low-level API of indirect instrumentor for foreign support******//
static int instrumentor_indirect_with_foreign_impl_init(
    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl,
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result = instrumentor_indirect_impl_init(
        &instrumentor_impl->base,
        operations_field_offset,
        operations_struct_size,
        replacements);
    
    if(result) return result;
    
    INIT_LIST_HEAD(&instrumentor_impl->foreign_instrumentor_impls);
    return 0;
}

static void instrumentor_indirect_with_foreign_impl_destroy(
    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl)
{
    while(!list_empty(&instrumentor_impl->foreign_instrumentor_impls))
    {
        struct foreign_instrumentor_indirect_impl* foreign_instrumentor_impl =
            list_first_entry(&instrumentor_impl->foreign_instrumentor_impls,
                            typeof(*foreign_instrumentor_impl),
                            list);

        list_del_init(&foreign_instrumentor_impl->list);
    }
    
    instrumentor_indirect_impl_destroy(&instrumentor_impl->base);
}


static struct foreign_instrumentor_indirect_impl*
instrumentor_indirect_impl_create_foreign_indirect(
    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl,
    size_t operations_field_offset,
    const struct kedr_coi_replacement* replacements)
{
    struct foreign_instrumentor_indirect_impl* foreign_instrumentor_impl =
        foreign_instrumentor_indirect_impl_create_disconnected(
            operations_field_offset,
            instrumentor_impl->base.operations_struct_size,
            replacements);
    
    if(foreign_instrumentor_impl == NULL) return NULL;
    
    list_add_tail(&foreign_instrumentor_impl->list,
        &instrumentor_impl->foreign_instrumentor_impls);
    
    foreign_instrumentor_impl->instrumentor_impl_binded = instrumentor_impl;
    
    return foreign_instrumentor_impl;
}
    
static int instrumentor_indirect_impl_replace_operations_from_prototype(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_object_data* object_data_new,
    void* object,
    struct instrumentor_indirect_object_data* prototype_data_impl)
{
    // Restore copy of operations
    instrumentor_indirect_impl_restore_ops(
        prototype_data_impl->ops_data,
        instrumentor_indirect_impl_object_ops_p_get(instrumentor_impl, object));
    
    return instrumentor_indirect_impl_replace_operations(instrumentor_impl,
        object_data_new,
        object);
}

static int instrumentor_indirect_impl_update_operations_from_prototype(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_object_data* object_data,
    void* object,
    struct instrumentor_indirect_object_data* prototype_data_impl)
{
    // Restore copy of operations
    instrumentor_indirect_impl_restore_ops(
        prototype_data_impl->ops_data,
        instrumentor_indirect_impl_object_ops_p_get(instrumentor_impl, object));
    
    return instrumentor_indirect_impl_update_operations(instrumentor_impl,
        object_data,
        object);
}

static int instrumentor_indirect_impl_chain_operations(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_impl* foreign_instrumentor_impl,
    void* object)
{
    unsigned long flags;
    struct instrumentor_indirect_ops_data* foreign_ops_data;
    
    const void** object_ops_p;
    
    object_ops_p = instrumentor_indirect_impl_object_ops_p_get(
        instrumentor_impl,
        object);
    
    spin_lock_irqsave(&foreign_instrumentor_impl->ops_lock, flags);
    foreign_ops_data = instrumentor_indirect_impl_find_ops_data(
        foreign_instrumentor_impl,
        *object_ops_p);
    
    // Restore copy of operations
    if((foreign_ops_data != NULL)
        && (*object_ops_p == foreign_ops_data->ops_repl))
    {
        *object_ops_p = foreign_ops_data->ops_orig;
    }

    spin_unlock_irqrestore(&foreign_instrumentor_impl->ops_lock, flags);
        
    return 0;
}


//****Interface of the indirect instrumentor with foreign support****//
static struct kedr_coi_object*
instrumentor_indirect_impl_iwf_iface_create_foreign_indirect(
    struct kedr_coi_object* iwf_iface_impl,
    size_t operations_field_offset,
    const struct kedr_coi_replacement* replacements)
{
    struct foreign_instrumentor_indirect_impl* foreign_instrumentor_impl;
    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl =
        container_of(iwf_iface_impl, typeof(*instrumentor_impl), base.base);
    
    foreign_instrumentor_impl =
        instrumentor_indirect_impl_create_foreign_indirect(
            instrumentor_impl,
            operations_field_offset,
            replacements);
    
    if(foreign_instrumentor_impl == NULL) return NULL;
    
    return &foreign_instrumentor_impl->base.base;
}

static int instrumentor_indirect_impl_iwf_iface_replace_operations_from_prototype(
    struct kedr_coi_object* iwf_iface_impl,
    struct kedr_coi_object* if_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object,
    struct kedr_coi_instrumentor_object_data* prototype_data,
    void* protorype_object,
    size_t operation_offset,
    void** op_repl)
{
    int result;

    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl =
        container_of(iwf_iface_impl, typeof(*instrumentor_impl), base.base);

    struct instrumentor_indirect_object_data* object_data_new_real =
        container_of(object_data_new, typeof(*object_data_new_real), base);
    struct instrumentor_indirect_object_data* prototype_data_real =
        container_of(prototype_data, typeof(*prototype_data_real), base);

    result = instrumentor_indirect_impl_replace_operations_from_prototype(
        &instrumentor_impl->base,
        object_data_new_real,
        object,
        prototype_data_real);
    
    *op_repl = get_operation_at_offset(
        *instrumentor_indirect_impl_object_ops_p_get(
            &instrumentor_impl->base,
            object),
        operation_offset);
    
    return result;
}

static int instrumentor_indirect_impl_iwf_iface_update_operations_from_prototype(
    struct kedr_coi_object* iwf_iface_impl,
    struct kedr_coi_object* if_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object,
    struct kedr_coi_instrumentor_object_data* prototype_data,
    void* protorype_object,
    size_t operation_offset,
    void** op_repl)
{
    int result;

    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl =
        container_of(iwf_iface_impl, typeof(*instrumentor_impl), base.base);

    struct instrumentor_indirect_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    struct instrumentor_indirect_object_data* prototype_data_real =
        container_of(prototype_data, typeof(*prototype_data_real), base);

    result = instrumentor_indirect_impl_update_operations_from_prototype(
        &instrumentor_impl->base,
        object_data_real,
        object,
        prototype_data_real);
    
    *op_repl = get_operation_at_offset(
        *instrumentor_indirect_impl_object_ops_p_get(
            &instrumentor_impl->base,
            object),
        operation_offset);
    
    return result;
}

static int instrumentor_indirect_impl_iwf_iface_chain_operation(
    struct kedr_coi_object* iwf_iface_impl,
    struct kedr_coi_object* if_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object,
    void* protorype_object,
    size_t operation_offset,
    void** op_repl)
{
    int result;

    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl =
        container_of(iwf_iface_impl, typeof(*instrumentor_impl), base.base);

    struct foreign_instrumentor_indirect_impl* foreign_instrumentor_impl =
        container_of(if_iface_impl, typeof(*foreign_instrumentor_impl), base.base);

    result = instrumentor_indirect_impl_chain_operations(
        &instrumentor_impl->base,
        &foreign_instrumentor_impl->base,
        object);
    
    *op_repl = get_operation_at_offset(
        *instrumentor_indirect_impl_object_ops_p_get(
            &instrumentor_impl->base,
            object),
        operation_offset);
    
    return result;
}


static void instrumentor_indirect_impl_iwf_iface_destroy_impl(
    struct kedr_coi_object* iwf_iface_impl)
{
    struct instrumentor_indirect_with_foreign_impl* impl_real =
        container_of(iwf_iface_impl, typeof(*impl_real), base.base);
    
    instrumentor_indirect_with_foreign_impl_destroy(impl_real);
    
    kfree(impl_real);
}

static struct kedr_coi_instrumentor_with_foreign_impl_iface
instrumentor_indirect_impl_iwf_iface =
{
    .in_iface =
    {
        .i_iface =
        {
            .alloc_object_data = instrumentor_indirect_impl_i_iface_alloc_object_data,
            .free_object_data = instrumentor_indirect_impl_i_iface_free_object_data,

            .replace_operations = instrumentor_indirect_impl_i_iface_replace_operations,
            .clean_replacement = instrumentor_indirect_impl_i_iface_clean_replacement,
            .update_operations = instrumentor_indirect_impl_i_iface_update_operations,
            
            .destroy_impl = instrumentor_indirect_impl_iwf_iface_destroy_impl
        },
        
        .get_orig_operation =
            instrumentor_indirect_impl_in_iface_get_orig_operation,
        .get_orig_operation_nodata =
            instrumentor_indirect_impl_in_iface_get_orig_operation_nodata
    },
    
    .create_foreign_indirect = instrumentor_indirect_impl_iwf_iface_create_foreign_indirect,
    .replace_operations_from_prototype =
        instrumentor_indirect_impl_iwf_iface_replace_operations_from_prototype,
    .update_operations_from_prototype =
        instrumentor_indirect_impl_iwf_iface_update_operations_from_prototype,
    .chain_operation = instrumentor_indirect_impl_iwf_iface_chain_operation
};

// Constructor for indirect instrumentor with foreign support
struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_instrumentor_with_foreign* instrumentor;
    
    int result;

    struct instrumentor_indirect_with_foreign_impl* instrumentor_impl;
    
    if((replacements == NULL) || (replacements[0].operation_offset == -1))
    {
        return instrumentor_with_foreign_empty_create();
    }
    
    instrumentor_impl = kmalloc(sizeof(*instrumentor_impl), GFP_KERNEL);
    
    if(instrumentor_impl == NULL)
    {
        pr_err("Failed to allocate structure for indirect instrumentor implementation with foreign support");
        return NULL;
    }
    
    result = instrumentor_indirect_with_foreign_impl_init(
        instrumentor_impl,
        operations_field_offset,
        operations_struct_size,
        replacements);
    if(result)
    {
        kfree(instrumentor_impl);
        return NULL;
    }

    instrumentor_impl->base.base.interface = &instrumentor_indirect_impl_iwf_iface.in_iface.i_iface.base;
    
    instrumentor = kedr_coi_instrumentor_with_foreign_create(&instrumentor_impl->base.base);
    
    if(instrumentor == NULL)
    {
        instrumentor_indirect_with_foreign_impl_destroy(instrumentor_impl);
        kfree(instrumentor_impl);
        return NULL;
    }

    return instrumentor;
}
//**Implementation of the global map for indirect operations fields***//
static struct kedr_coi_hash_table indirect_operations_fields;
DEFINE_SPINLOCK(indirect_operations_fields_lock);

int indirect_operations_fields_add_unique(
    struct kedr_coi_hash_elem* operations_field_elem)
{
    unsigned long flags;

    spin_lock_irqsave(&indirect_operations_fields_lock, flags);
    if(kedr_coi_hash_table_find_elem(&indirect_operations_fields,
        operations_field_elem->key))
    {
        spin_unlock_irqrestore(&indirect_operations_fields_lock, flags);
        return -EBUSY;
    }
    
    kedr_coi_hash_table_add_elem(&indirect_operations_fields,
        operations_field_elem);
    spin_unlock_irqrestore(&indirect_operations_fields_lock, flags);
    return 0;
}

void indirect_operations_fields_remove(
    struct kedr_coi_hash_elem* operations_field_elem)
{
    unsigned long flags;
    
    spin_lock_irqsave(&indirect_operations_fields_lock, flags);
    kedr_coi_hash_table_remove_elem(&indirect_operations_fields,
        operations_field_elem);
    spin_unlock_irqrestore(&indirect_operations_fields_lock, flags);
}

//**Implementation of the global map for direct objects***//
static struct kedr_coi_hash_table objects_directly_instrumented;

DEFINE_SPINLOCK(objects_directly_instrumented_lock);

int objects_directly_instrumented_add_unique(
    struct kedr_coi_hash_elem* object_elem)
{
    unsigned long flags;
    
    spin_lock_irqsave(&objects_directly_instrumented_lock, flags);
    if(kedr_coi_hash_table_find_elem(&objects_directly_instrumented,
        object_elem->key))
    {
        spin_unlock_irqrestore(&objects_directly_instrumented_lock, flags);
        return -EBUSY;
    }
    kedr_coi_hash_table_add_elem(&objects_directly_instrumented,
        object_elem);
    
    spin_unlock_irqrestore(&objects_directly_instrumented_lock, flags);

    return 0;
}

void objects_directly_instrumented_remove(
    struct kedr_coi_hash_elem* object_elem)
{
    unsigned long flags;
    
    spin_lock_irqsave(&objects_directly_instrumented_lock, flags);
    kedr_coi_hash_table_remove_elem(&objects_directly_instrumented,
        object_elem);
    spin_unlock_irqrestore(&objects_directly_instrumented_lock, flags);
}


//*******Initialize and destroy instrumentors support*****************//
int kedr_coi_instrumentors_init(void)
{
    int result;
    
    result = kedr_coi_hash_table_init(&indirect_operations_fields);
    if(result) goto err_indirect;
    
    result = kedr_coi_hash_table_init(&objects_directly_instrumented);
    if(result) goto err_direct;
    
    return 0;

err_direct:
    kedr_coi_hash_table_destroy(&indirect_operations_fields, NULL);
err_indirect:
    return result;
}

void kedr_coi_instrumentors_destroy(void)
{
    kedr_coi_hash_table_destroy(&objects_directly_instrumented, NULL);
    kedr_coi_hash_table_destroy(&indirect_operations_fields,
        /*&indirect_operations_fields_trace_undeleted*/NULL);
}

