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

/*
// For debug
#ifdef spin_lock_irqsave
#undef spin_lock_irqsave
#endif

#define spin_lock_irqsave(lock, flags) do {flags = 1;} while(0);

#ifdef spin_unlock_irqrestore
#undef spin_unlock_irqrestore
#endif

#define spin_unlock_irqrestore(lock, flags) do {} while(0);
*/

/** Helpers for simplify access to operation */

/*
 * Get operation at given offset in the operations structure.
 */
static inline void*
operation_at_offset(const void* ops, size_t operation_offset)
{
    if(ops == NULL) return NULL;
    return *((void**)((const char*)ops + operation_offset));
}

/*
 * Get operation at given offset in the operations structure.
 * Operations pointer cannot be NULL.
 * 
 */
static inline void*
operation_at_offset_not_null(const void* ops, size_t operation_offset)
{
    BUG_ON(ops != NULL);
    return *((void**)((const char*)ops + operation_offset));
}


/*
 * Get reference to operation at given offset in the operations
 * structure.
 */

static inline void**
operation_at_offset_p(void* ops, size_t operation_offset)
{
    return ((void**)((char*)ops + operation_offset));
}

//********* Helpers for simplify operation replacement*****************/

/*
 * Replace operation according to replacement structure.
 * 
 * Note: original operation should be stored before in any way.
 */
static void replace_operation(void** op_p,
    const struct kedr_coi_replacement* replacement)
{
    if(replacement->mode == replace_all)
    {
        *op_p = replacement->repl;
    }
    else if(*op_p)
    {
        if(replacement->mode == replace_not_null) *op_p = replacement->repl;
    }
    else
    {
        if(replacement->mode == replace_null) *op_p = replacement->repl;
    }
}

/*
 * Update replacement of the operation.
 * 
 * Also update stored original operation if needed.
 */
static void update_operation(void** op_p,
    const struct kedr_coi_replacement* replacement,
    void** op_orig)
{
    if(replacement->repl != *op_p)
    {
        *op_orig = *op_p;
        replace_operation(op_p, replacement);
    }
}

/*
 * Restore one operation if it was replaced.
 */
static void restore_operation(void** op_p,
    const struct kedr_coi_replacement* replacement,
    void* op_orig)
{
#ifdef KEDR_COI_DEBUG
    if((replacement->mode == replace_not_null) && !op_orig);
    else if((replacement->mode == replace_null) && op_orig);
    else if(replacement->repl != *op_p)
    {
        pr_info("Operation was changed externally from %pF to %pF.",
            replacement->repl, *op_p);
    }
#endif /* KEDR_COI_DEBUG */
    if(replacement->repl == *op_p) *op_p = op_orig;
}

/*
 * Return operation before replace when no stored original operations
 * has found.
 * 
 * Return ERR_PTR() if original operation cannot be reconstructed.
 */
static void* get_orig_operation_nodata(const void* ops,
    size_t operation_offset,
    const struct kedr_coi_replacement* replacements)
{
    const struct kedr_coi_replacement* replacement;
    void* op = operation_at_offset(ops, operation_offset);
    for(replacement = replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        if(replacement->operation_offset != operation_offset) continue;
        
        if(op != replacement->repl) return op;
        if(replacement->mode == replace_null) return NULL;
        return ERR_PTR(-ENODEV);
    }
    return op;
}

/*
 * Return replaced operation. Usefull when operation has been replaced
 * again.
 */
static void* get_repl_operation(
    const struct kedr_coi_replacement* replacement,
    void* op_orig)
{
    void* op = op_orig;
    replace_operation(&op, replacement);
    return op;
}

/*
 * Replace all operations in the struct according to replacement
 * structures.
 */
static void replace_operations(void* ops,
    const struct kedr_coi_replacement* replacements)
{
    const struct kedr_coi_replacement* replacement;
    for(replacement = replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        void** operation_p = operation_at_offset_p(ops,
            replacement->operation_offset);

        replace_operation(operation_p, replacement);
    }
}

/*
 * Replace all operations in the struct according to replacement
 * structures. Also store operations which are needed for restore them.
 */
static void replace_operations_store(void* ops,
    const struct kedr_coi_replacement* replacements, void* ops_old)
{
    const struct kedr_coi_replacement* replacement;
    for(replacement = replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        void** operation_p = operation_at_offset_p(ops,
            replacement->operation_offset);
        *operation_at_offset_p(ops_old, replacement->operation_offset) =
            *operation_p;

        replace_operation(operation_p, replacement);
    }
}

/*
 * Restore operations according to replacements structures
 * and operations stored.
 * 
 * NOTE: ops_orig should contain full copy of operations or at least one
 * resulted from replace_operations_store().
 */
static void restore_operations(void* ops,
    const struct kedr_coi_replacement* replacements, const void* ops_orig)
{
    const struct kedr_coi_replacement* replacement;
    for(replacement = replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        size_t operation_offset = replacement->operation_offset;
        restore_operation(
            operation_at_offset_p(ops, operation_offset),
            replacement,
            operation_at_offset(ops_orig, operation_offset));
    }
}
/**********************************************************************/
/***********Instrumentor which does not instrument at all**************/
/**********************************************************************/

/*
 * Such instrumentor is a fallback for instrumentor's constructors
 * when nothing to replace.
 */

// Interface of emtpy instrumentor as common one
static struct kedr_coi_instrumentor_watch_data*
instrumentor_empty_impl_iface_alloc_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    return kmalloc(sizeof(struct kedr_coi_instrumentor_watch_data), GFP_KERNEL);
}

static void instrumentor_empty_impl_iface_free_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    kfree(watch_data);
}

static int instrumentor_empty_impl_iface_replace_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    return 0;// nothing to replace
}

static void instrumentor_empty_impl_iface_clean_replacement(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
}


static int instrumentor_empty_impl_iface_update_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    return 0;// always up-to-date
}

static void instrumentor_empty_impl_iface_destroy_impl(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    kfree(iface_impl);
}

struct kedr_coi_instrumentor_impl_iface instrumentor_empty_iface =
{
    .alloc_watch_data = instrumentor_empty_impl_iface_alloc_watch_data,
    .free_watch_data = instrumentor_empty_impl_iface_free_watch_data,
    
    .replace_operations = instrumentor_empty_impl_iface_replace_operations,
    .clean_replacement = instrumentor_empty_impl_iface_clean_replacement,
    .update_operations = instrumentor_empty_impl_iface_update_operations,
    .destroy_impl = instrumentor_empty_impl_iface_destroy_impl
};

// Constructor for empty instrumentor(for internal use)
static struct kedr_coi_instrumentor*
instrumentor_empty_create(void)
{
    struct kedr_coi_instrumentor* instrumentor;
    struct kedr_coi_instrumentor_impl* instrumentor_empty_impl = 
        kmalloc(sizeof(*instrumentor_empty_impl), GFP_KERNEL);
    
    if(instrumentor_empty_impl == NULL)
    {
        pr_err("Failed to allocate structure for empty instrumentor.");
        return NULL;
    }
    
    instrumentor_empty_impl->interface = &instrumentor_empty_iface;

    instrumentor = kedr_coi_instrumentor_create(instrumentor_empty_impl);
    if(instrumentor == NULL)
    {
        kfree(instrumentor_empty_impl);
        return NULL;
    }
    
    return instrumentor;
}


//*********** Empty instrumentor with foreign support*****************//
static struct kedr_coi_foreign_instrumentor_watch_data*
instrumentor_empty_f_impl_iface_alloc_watch_data(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl)
{
    return kmalloc(sizeof(struct kedr_coi_foreign_instrumentor_watch_data), GFP_KERNEL);
}

static void instrumentor_empty_f_impl_iface_free_watch_data(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data)
{
    kfree(watch_data);
}

static int instrumentor_empty_f_impl_iface_replace_operations(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    return 0;// nothing to replace
}

static void instrumentor_empty_f_impl_iface_clean_replacement(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
}


static int instrumentor_empty_f_impl_iface_update_operations(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    return 0;// always up-to-date
}

static void instrumentor_empty_f_impl_iface_destroy_impl(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl)
{
    kfree(iface_impl);
}


static struct kedr_coi_foreign_instrumentor_impl_iface
instrumentor_empty_f_impl_iface =
{
    .alloc_watch_data = instrumentor_empty_f_impl_iface_alloc_watch_data,
    .free_watch_data = instrumentor_empty_f_impl_iface_free_watch_data,
    
    .replace_operations = instrumentor_empty_f_impl_iface_replace_operations,
    .clean_replacement = instrumentor_empty_f_impl_iface_clean_replacement,
    .update_operations = instrumentor_empty_f_impl_iface_update_operations,
    
    .destroy_impl = instrumentor_empty_f_impl_iface_destroy_impl
};

static struct kedr_coi_instrumentor_watch_data*
instrumentor_empty_wf_impl_iface_alloc_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    return kmalloc(sizeof(struct kedr_coi_instrumentor_watch_data), GFP_KERNEL);
}

static void instrumentor_empty_wf_impl_iface_free_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    kfree(watch_data);
}

static int instrumentor_empty_wf_impl_iface_replace_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    return 0;// nothing to replace
}

static void instrumentor_empty_wf_impl_iface_clean_replacement(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
}


static int instrumentor_empty_wf_impl_iface_update_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    return 0;// always up-to-date
}

static void instrumentor_empty_wf_impl_iface_destroy_impl(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    kfree(iface_impl);
}

static struct kedr_coi_foreign_instrumentor_impl*
instrumentor_empty_wf_impl_iface_foreign_instrumentor_impl_create(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_foreign_instrumentor_impl* foreign_instrumentor_empty_impl =
        kmalloc(sizeof(*foreign_instrumentor_empty_impl), GFP_KERNEL);
    
    if(foreign_instrumentor_empty_impl == NULL)
    {
        pr_err("Failed to allocate structure for empty foreign instrumentor");
        return NULL;
    }
    
    foreign_instrumentor_empty_impl->interface =
        &instrumentor_empty_f_impl_iface;
    
    return foreign_instrumentor_empty_impl;
}


static struct kedr_coi_instrumentor_with_foreign_impl_iface
instrumentor_empty_wf_impl_iface =
{
    .base =
    {
        .alloc_watch_data = instrumentor_empty_wf_impl_iface_alloc_watch_data,
        .free_watch_data = instrumentor_empty_wf_impl_iface_free_watch_data,
        
        .replace_operations = instrumentor_empty_wf_impl_iface_replace_operations,
        .clean_replacement = instrumentor_empty_wf_impl_iface_clean_replacement,
        .update_operations = instrumentor_empty_wf_impl_iface_update_operations,
        
        .destroy_impl = instrumentor_empty_wf_impl_iface_destroy_impl
        // Other fields should not be accesssed
    },
    .foreign_instrumentor_impl_create =
        instrumentor_empty_wf_impl_iface_foreign_instrumentor_impl_create,
    // Other fields should not be accesssed
};

static struct kedr_coi_instrumentor_with_foreign*
instrumentor_with_foreign_empty_create(void)
{
    struct kedr_coi_instrumentor_with_foreign* instrumentor;
    struct kedr_coi_instrumentor_impl* instrumentor_empty_wf_impl = 
        kmalloc(sizeof(*instrumentor_empty_wf_impl), GFP_KERNEL);
    
    if(instrumentor_empty_wf_impl == NULL)
    {
        pr_err("Failed to allocate structure for empty instrumentor.");
        return NULL;
    }
    
    instrumentor_empty_wf_impl->interface =
        &instrumentor_empty_wf_impl_iface.base;

    instrumentor = kedr_coi_instrumentor_with_foreign_create(instrumentor_empty_wf_impl);
    if(instrumentor == NULL)
    {
        kfree(instrumentor_empty_wf_impl);
        return NULL;
    }
    
    return instrumentor;
}

/*********************************************************************/
/********Instrumentor for direct operations (rarely used)*************/
/*********************************************************************/

/*
 * Global map for prevent different direct instrumentors to perform
 * operations replacements which has undesireable effect on each other.
 * 
 * In this concrete case, map contains pointers to operations.
 */

/*
 * Add operations(as key) to the hash table of operations instrumented.
 * 
 * If operations already exists in the table, return -EBUSY.
 */
static int operations_directly_instrumented_add_unique(
    struct kedr_coi_hash_elem* hash_elem_operations);

/*
 * Remove object(as key) from the hash table of objects instrumented*/
static void operations_directly_instrumented_remove(
    struct kedr_coi_hash_elem* hash_elem_operations);

// Per-object data of direct instrumentor implementation.
struct instrumentor_direct_watch_data
{
    struct kedr_coi_instrumentor_watch_data base;

    struct kedr_coi_hash_elem hash_elem_operations;
    // original values for replaced operations(other fields are uninitialized).
    void* ops_orig;
};

// Object type of direct instrumentor implementation.
struct instrumentor_direct_impl
{
    struct kedr_coi_instrumentor_impl base;
    size_t operations_struct_size;
    /*
     *  NOTE: 'replacements' field is not NULL.
     * 
     * (without replacements empty variant of instrumentor is created).
     */
    const struct kedr_coi_replacement* replacements;
};


/*
 * Low level API of the direct instrumentor implementation.
 */

/*
 * Replace operations, filling watch data for it.
 */
static int
instrumentor_direct_impl_replace_operations(
    struct instrumentor_direct_impl* instrumentor_impl,
    struct instrumentor_direct_watch_data* watch_data_new,
    void* ops)
{
    int result;
    
    // Check collisions with other instrumentors.
    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_operations, ops);
    
    result = operations_directly_instrumented_add_unique(
        &watch_data_new->hash_elem_operations);

    switch(result)
    {
    case 0:
        break;
    case -EBUSY:
        pr_err("Direct watching for operations %p failed because them "
            "are already watched by another interceptor.", ops);
        return -EBUSY;
    default:
        return result;
    }

    // Replace operations in the objects and store its old values.
    replace_operations_store(ops, instrumentor_impl->replacements,
        watch_data_new->ops_orig);
    
    return 0;
}

/*
 *  Clean operations replacement and restore operation(if not NULL).
 */
static void
instrumentor_direct_impl_clean_replacement(
    struct instrumentor_direct_impl* instrumentor_impl,
    struct instrumentor_direct_watch_data* watch_data,
    void* ops)
{
    if(ops)
    {
        restore_operations(ops, instrumentor_impl->replacements,
            watch_data->ops_orig);
    }
    
    operations_directly_instrumented_remove(&watch_data->hash_elem_operations);
}


/*
 * Update replaced operations in the object.
 */
static int
instrumentor_direct_impl_update_operations(
    struct instrumentor_direct_impl* instrumentor_impl,
    struct instrumentor_direct_watch_data* watch_data,
    void* ops)
{
    const struct kedr_coi_replacement* replacement;
    char* ops_orig = watch_data->ops_orig;
        
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        size_t operation_offset = replacement->operation_offset;
        void** operation_p = operation_at_offset_p(
            ops, operation_offset);
         
        update_operation(operation_p, replacement, 
            operation_at_offset_p(ops_orig, operation_offset));
    }
    
    return 0;
}


static void*
instrumentor_direct_impl_get_orig_operation(
    struct instrumentor_direct_impl* instrumentor_impl,
    struct instrumentor_direct_watch_data* watch_data,
    size_t operation_offset)
{
    return operation_at_offset(watch_data->ops_orig, operation_offset);
}

//************ Interface of direct instrumentor ***********************
static struct kedr_coi_instrumentor_watch_data*
instrumentor_direct_impl_iface_alloc_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    struct instrumentor_direct_watch_data* watch_data;
    
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    watch_data = kmalloc(sizeof(*watch_data), GFP_KERNEL);
    if(watch_data == NULL) return NULL;
    
    watch_data->ops_orig = kmalloc(
        instrumentor_impl->operations_struct_size, GFP_KERNEL);
    if(watch_data->ops_orig == NULL)
    {
        kfree(watch_data);
        return NULL;
    }
    
    return &watch_data->base;
}

static void
instrumentor_direct_impl_iface_free_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    struct instrumentor_direct_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    kfree(watch_data_real->ops_orig);
    kfree(watch_data_real);
}

static int
instrumentor_direct_impl_iface_replace_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_direct_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);

    return instrumentor_direct_impl_replace_operations(
        instrumentor_impl,
        watch_data_real,
        (void*)*ops_p);
}

static void
instrumentor_direct_impl_iface_clean_replacement(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_direct_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_direct_impl_clean_replacement(
        instrumentor_impl,
        watch_data_real,
        ops_p ? (void*)*ops_p : NULL);
}


static int
instrumentor_direct_impl_iface_update_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_direct_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_direct_impl_update_operations(
        instrumentor_impl,
        watch_data_real,
        (void*)*ops_p);
}

static void*
instrumentor_direct_impl_iface_get_orig_operation(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void* ops,
    size_t operation_offset)
{
    struct instrumentor_direct_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_direct_impl_get_orig_operation(
        instrumentor_impl,
        watch_data_real,
        operation_offset);
}

static void*
instrumentor_direct_impl_iface_get_orig_operation_nodata(
    struct kedr_coi_instrumentor_impl* iface_impl,
    const void* ops,
    size_t operation_offset)
{
    //Shouldn't be called
    BUG();
    
    return ERR_PTR(-EINVAL);
}


static void instrumentor_direct_impl_iface_destroy_impl(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    kfree(instrumentor_impl);
}

static struct kedr_coi_instrumentor_impl_iface
instrumentor_direct_impl_iface =
{
    .alloc_watch_data = instrumentor_direct_impl_iface_alloc_watch_data,
    .free_watch_data = instrumentor_direct_impl_iface_free_watch_data,

    .replace_operations = instrumentor_direct_impl_iface_replace_operations,
    .clean_replacement = instrumentor_direct_impl_iface_clean_replacement,
    .update_operations = instrumentor_direct_impl_iface_update_operations,
    
    .destroy_impl = instrumentor_direct_impl_iface_destroy_impl,

    .get_orig_operation = instrumentor_direct_impl_iface_get_orig_operation,
    .get_orig_operation_nodata = instrumentor_direct_impl_iface_get_orig_operation_nodata
};


//Constructor for direct instrumentor
static struct kedr_coi_instrumentor*
instrumentor_direct_create(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_instrumentor* instrumentor;
    
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
    
    instrumentor_impl->operations_struct_size = operations_struct_size;
    instrumentor_impl->replacements = replacements;

    instrumentor_impl->base.interface = &instrumentor_direct_impl_iface;
    
    instrumentor = kedr_coi_instrumentor_create(&instrumentor_impl->base);
    
    if(instrumentor == NULL)
    {
        kfree(instrumentor_impl);
        return NULL;
    }

    return instrumentor;
}


/**********************************************************************/
/*********Instrumentor for indirect operations (mostly used)***********/
/**********************************************************************/


/*
 * Global map for prevent different indirect instrumentors to perform
 * operations replacements which has undesireable effect on each other.
 * 
 * In this concrete case, map contains reference to pointers to operations.
 */

/*
 * Add reference to the operation field(as key) to the map of
 * instrumented operations fields.
 * 
 * If field is already in the map, return -EBUSY.
 */
static int indirect_operations_fields_instrumented_add_unique(
    struct kedr_coi_hash_elem* hash_elem_operations_field);

static void indirect_operations_fields_instrumented_remove(
    struct kedr_coi_hash_elem* hash_elem_operations_field);


// Need for "self-foreign" replacements
static void indirect_operations_fields_instrumented_transmit(
    struct kedr_coi_hash_elem* hash_elem_operations_field,
    struct kedr_coi_hash_elem* hash_elem_operations_field_new);


/*
 * Per-operations data for normal indirect instrumentor.
 */
struct instrumentor_indirect_ops_data
{
    // Pointer to operations, for which these operations data was created
    const void* ops_orig;
    // Allocated buffer for replaced operations
    void* ops_repl;
    //key - replaced operations
    struct kedr_coi_hash_elem hash_elem_ops_repl;
    //key - original operations
    struct kedr_coi_hash_elem hash_elem_ops_orig;
    // refcount for share these data
    int refs;
};

/*
 *  Watch data for instrumentor.
 * 
 * Contain reference to per-operations data plus key for global map
 * of instrumented operations fields.
 */

struct instrumentor_indirect_watch_data
{
    struct kedr_coi_instrumentor_watch_data base;

    /*
     *  Element for global map preventing reference to operations to be
     * instrumented by another instrumentor.
     */
    struct kedr_coi_hash_elem hash_elem_operations_field;

    struct instrumentor_indirect_ops_data* ops_data;
};

/*
 * Implementation of the indirect instrumentor.
 */
struct instrumentor_indirect_impl
{
    struct kedr_coi_instrumentor_impl base;

    // hash table with replaced operations as keys
    struct kedr_coi_hash_table hash_table_ops_repl;
    // hash table with original operations as keys
    struct kedr_coi_hash_table hash_table_ops_orig;

    /*
     * Protect tables of operations data from concurrent access.
     * Also protect 'refs' field of per-operations data from concurrent
     * access.
     */
    spinlock_t ops_lock;
    
    size_t operations_struct_size;
    /*
     *  NOTE: 'replacements' field is not NULL
     * (without replacements empty variant of instrumentor is created).
     */
    const struct kedr_coi_replacement* replacements;
};


/*
 * Return per-operations data with given replaced operations.
 * If such data not found, return NULL.
 * 
 * Should be executed under lock.
 */
static struct instrumentor_indirect_ops_data*
instrumentor_indirect_impl_find_ops_data(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* ops_repl)
{
    struct kedr_coi_hash_elem* hash_elem_ops_repl =
        kedr_coi_hash_table_find_elem(&instrumentor_impl->hash_table_ops_repl,
        ops_repl);
    
    return hash_elem_ops_repl
        ? container_of(hash_elem_ops_repl,
            struct instrumentor_indirect_ops_data,
            hash_elem_ops_repl)
        : NULL;
}

/*
 * Return per-operations data with given original operations.
 * If such data not found, return NULL.
 * 
 * Should be executed under lock.
 */
static struct instrumentor_indirect_ops_data*
instrumentor_indirect_impl_find_ops_data_orig(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* ops_orig)
{
    struct kedr_coi_hash_elem* hash_elem_ops_orig =
        kedr_coi_hash_table_find_elem(&instrumentor_impl->hash_table_ops_orig,
        ops_orig);
    
    return hash_elem_ops_orig
        ? container_of(hash_elem_ops_orig,
            struct instrumentor_indirect_ops_data,
            hash_elem_ops_orig)
        : NULL;
}

/*
 * Allocate instance of per-operations data, fill its fields and
 * add to the hash tables of original and replaced operations.
 * 
 * Should be executed under lock.
 */
static struct instrumentor_indirect_ops_data*
instrumentor_indirect_impl_add_ops_data(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* ops_orig)
{
    struct instrumentor_indirect_ops_data* ops_data =
        kmalloc(sizeof(*ops_data), GFP_ATOMIC);
    
    if(ops_data == NULL) goto err;
    
    ops_data->ops_orig = ops_orig;
    
    ops_data->ops_repl =
        kmalloc(instrumentor_impl->operations_struct_size, GFP_ATOMIC);
    
    if(ops_data->ops_repl == NULL) goto ops_repl_err;

    kedr_coi_hash_elem_init(&ops_data->hash_elem_ops_orig, ops_orig);
    if(kedr_coi_hash_table_add_elem(
        &instrumentor_impl->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig))
    {
        goto hash_elem_ops_orig_err;
    }
    
    
    kedr_coi_hash_elem_init(&ops_data->hash_elem_ops_repl,
        ops_data->ops_repl);
    if(kedr_coi_hash_table_add_elem(
        &instrumentor_impl->hash_table_ops_repl,
        &ops_data->hash_elem_ops_repl))
    {
        goto hash_elem_ops_repl_err;
    }

    if(ops_orig)
    {
        memcpy(ops_data->ops_repl, ops_orig,
            instrumentor_impl->operations_struct_size);
    }
    else
    {
        /* 
         * NULL pointer to operations is equvalent to zeroes for all
         * operations.
         */
        memset(ops_data->ops_repl, 0,
            instrumentor_impl->operations_struct_size);
    }
    replace_operations(ops_data->ops_repl,
        instrumentor_impl->replacements);
    
    ops_data->refs = 1;
    
    return ops_data;


hash_elem_ops_repl_err:
    kedr_coi_hash_table_remove_elem(
        &instrumentor_impl->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig);
hash_elem_ops_orig_err:
    kfree(ops_data->ops_repl);
ops_repl_err:
    kfree(ops_data);
err:
    return NULL;
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
    ++ops_data->refs;
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
    if(--ops_data->refs > 0) return;

    kedr_coi_hash_table_remove_elem(
        &instrumentor_impl->hash_table_ops_repl,
        &ops_data->hash_elem_ops_repl);

    kedr_coi_hash_table_remove_elem(
        &instrumentor_impl->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig);
    
    kfree(ops_data->ops_repl);
    kfree(ops_data);
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
    const void* ops)
{
    struct instrumentor_indirect_ops_data* ops_data;
    
    // Look for existed per-operations data for that operations
    ops_data = instrumentor_indirect_impl_find_ops_data(
        instrumentor_impl,
        ops);
    
    if(!ops_data)
    {
        ops_data = instrumentor_indirect_impl_find_ops_data_orig(
            instrumentor_impl,
            ops);
    }
    
    if(ops_data)
    {
        instrumentor_indirect_impl_ref_ops_data(
            instrumentor_impl, ops_data);
        return ops_data;
    }

    return instrumentor_indirect_impl_add_ops_data(
        instrumentor_impl, ops);
}

//***************** Low level API of indirect instrumentor***********//

static int
instrumentor_indirect_impl_replace_operations(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_watch_data* watch_data_new,
    const void** ops_p)
{
    unsigned long flags;
    struct instrumentor_indirect_ops_data* ops_data;
    
    int result;

    /* 
     * Check for collisions with another instrumentors.
     */

    kedr_coi_hash_elem_init(
        &watch_data_new->hash_elem_operations_field, ops_p);

    result = indirect_operations_fields_instrumented_add_unique(
        &watch_data_new->hash_elem_operations_field);
    
    if(result)
    {
        if(result == -EBUSY)
        {
            pr_err("Indirect watching for operations at %p failed "
                "because them are already watched by another interceptor.",
                ops_p);
        }
        return result;
    }

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    ops_data = instrumentor_indirect_impl_get_ops_data(
        instrumentor_impl,
        *ops_p);
    
    if(ops_data == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);

        indirect_operations_fields_instrumented_remove(
            &watch_data_new->hash_elem_operations_field);
        return -ENOMEM;
    }

    watch_data_new->ops_data = ops_data;

    *ops_p = ops_data->ops_repl;

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return 0;
}

static void
instrumentor_indirect_impl_clean_replacement(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_watch_data* watch_data,
    const void** ops_p)
{
    unsigned long flags;
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    if(ops_p && (*ops_p == watch_data->ops_data->ops_repl))
    {
        *ops_p = watch_data->ops_data->ops_orig;
    }

    instrumentor_indirect_impl_unref_ops_data(instrumentor_impl,
        watch_data->ops_data);
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    indirect_operations_fields_instrumented_remove(
        &watch_data->hash_elem_operations_field);
}

static int
instrumentor_indirect_impl_update_operations(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_watch_data* watch_data,
    const void** ops_p)
{
    unsigned long flags;
    struct instrumentor_indirect_ops_data* ops_data;
    struct instrumentor_indirect_ops_data* ops_data_new;
    
    ops_data = watch_data->ops_data;
    
    /* 
     * 'ops_data' is reffered by current object, access to which is
     * serialized by the caller.
     * 
     * So, lock doesn't need for access to operations data's
     * final fields.
     */
    if(*ops_p == ops_data->ops_repl)
    {
        //up-to-date
        return 0;
    }
    
    if(*ops_p == ops_data->ops_orig)
    {
        //set replaced operations again
        *ops_p = ops_data->ops_repl;
        return 0;
    }
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    ops_data_new = instrumentor_indirect_impl_get_ops_data(
        instrumentor_impl,
        *ops_p);
    
    if(ops_data_new == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        return -ENOMEM;
    }
    
    watch_data->ops_data = ops_data_new;
    
    *ops_p = ops_data_new->ops_repl;
    
    instrumentor_indirect_impl_unref_ops_data(
        instrumentor_impl, ops_data);

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return 0;
}

static void* instrumentor_indirect_impl_get_orig_operation_nodata(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* ops,
    size_t operation_offset)
{
    void* op_orig;
    unsigned long flags;
    struct instrumentor_indirect_ops_data* ops_data;
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    ops_data = instrumentor_indirect_impl_find_ops_data(
        instrumentor_impl, ops);

    if(!ops_data)
    {
        ops_data = instrumentor_indirect_impl_find_ops_data_orig(
            instrumentor_impl, ops);
    }
        
    op_orig = ops_data
        ? operation_at_offset(ops_data->ops_orig, operation_offset)
        : get_orig_operation_nodata(ops, operation_offset,
            instrumentor_impl->replacements);
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return op_orig;
}

//*************Interface of the indirect instrumentor****************

static struct kedr_coi_instrumentor_watch_data*
instrumentor_indirect_impl_iface_alloc_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    struct instrumentor_indirect_watch_data* watch_data =
        kmalloc(sizeof(*watch_data), GFP_KERNEL);

    return watch_data ? &watch_data->base : NULL;
}

static void
instrumentor_indirect_impl_iface_free_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    struct instrumentor_indirect_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real),
            base);
    
    kfree(watch_data_real);
}


static int
instrumentor_indirect_impl_iface_replace_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    struct instrumentor_indirect_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    struct instrumentor_indirect_watch_data* watch_data_new_real =
        container_of(watch_data_new, typeof(*watch_data_new_real), base);
    
    return instrumentor_indirect_impl_replace_operations(
        instrumentor_impl,
        watch_data_new_real,
        ops_p);
}

static void
instrumentor_indirect_impl_iface_clean_replacement(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    struct instrumentor_indirect_impl* instrumentor_impl = 
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_indirect_impl_clean_replacement(
        instrumentor_impl,
        watch_data_real,
        ops_p);
}

static int
instrumentor_indirect_impl_iface_update_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    struct instrumentor_indirect_impl* instrumentor_impl = 
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_indirect_impl_update_operations(
        instrumentor_impl,
        watch_data_real,
        ops_p);
}

static void* instrumentor_indirect_impl_iface_get_orig_operation(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void* ops,
    size_t operation_offset)
{
    struct instrumentor_indirect_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    return operation_at_offset(
        watch_data_real->ops_data->ops_orig,
        operation_offset);
}

static void*
instrumentor_indirect_impl_iface_get_orig_operation_nodata(
    struct kedr_coi_instrumentor_impl* iface_impl,
    const void* ops,
    size_t operation_offset)
{
    struct instrumentor_indirect_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_indirect_impl_get_orig_operation_nodata(
        instrumentor_impl,
        ops,
        operation_offset);
}

static void instrumentor_indirect_impl_iface_destroy_impl(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    struct instrumentor_indirect_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_repl,
        NULL);
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_orig,
        NULL);

    kfree(instrumentor_impl);
}

static struct kedr_coi_instrumentor_impl_iface instrumentor_indirect_impl_iface =
{
    .alloc_watch_data = instrumentor_indirect_impl_iface_alloc_watch_data,
    .free_watch_data = instrumentor_indirect_impl_iface_free_watch_data,

    .replace_operations = instrumentor_indirect_impl_iface_replace_operations,
    .clean_replacement = instrumentor_indirect_impl_iface_clean_replacement,
    .update_operations = instrumentor_indirect_impl_iface_update_operations,

    .destroy_impl = instrumentor_indirect_impl_iface_destroy_impl,
    
    .get_orig_operation = instrumentor_indirect_impl_iface_get_orig_operation,
    .get_orig_operation_nodata = instrumentor_indirect_impl_iface_get_orig_operation_nodata
};


// Constructor for indirect instrumentor
static struct kedr_coi_instrumentor*
instrumentor_indirect_create(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_instrumentor* instrumentor;
    
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
        goto err;
    }
    
    result = kedr_coi_hash_table_init(
        &instrumentor_impl->hash_table_ops_orig);
    if(result) goto hash_table_ops_orig_err;

    result = kedr_coi_hash_table_init(
        &instrumentor_impl->hash_table_ops_repl);
    if(result) goto hash_table_ops_repl_err;

    instrumentor_impl->operations_struct_size = operations_struct_size;
    instrumentor_impl->replacements = replacements;
    
    spin_lock_init(&instrumentor_impl->ops_lock);
    
    instrumentor_impl->base.interface = &instrumentor_indirect_impl_iface;
    
    instrumentor = kedr_coi_instrumentor_create(&instrumentor_impl->base);
    
    if(instrumentor == NULL) goto instrumentor_err;

    return instrumentor;

instrumentor_err:
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_repl,
        NULL);
hash_table_ops_repl_err:
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_orig,
        NULL);
hash_table_ops_orig_err:
    kfree(instrumentor_impl);
err:
    return NULL;
}


//*********Indirect instrumentor with foreign support***************//
struct instrumentor_indirect_wf_impl;
struct instrumentor_indirect_f_impl;

struct instrumentor_indirect_wf_ops_data;
struct instrumentor_indirect_f_ops_data;

struct instrumentor_indirect_wf_ops_data
{
    const void* ops_orig;
    void* ops_repl;

    struct kedr_coi_hash_elem hash_elem_ops_orig;
    struct kedr_coi_hash_elem hash_elem_ops_repl;
    
    int refs;
    // List of foreign operations data created for these.
    struct list_head foreign_ops_data_list;
};

struct instrumentor_indirect_f_ops_data
{
    void* ops_repl;
    struct kedr_coi_hash_elem hash_elem_ops_repl;
    
    int refs;
    // Foreign instrumentor which create these data
    struct instrumentor_indirect_f_impl* instrumentor_impl;
    /*
     * Reference to the normal operations data.
     * 
     * ops_repl contents same as binded_ops_data->ops_repl +
     * foreign replacements, so it also fits for self-foreign watching.
     */
    struct instrumentor_indirect_wf_ops_data* binded_ops_data;
    // List organization of foreign operations data for same binded data.
    struct list_head list;
};

struct instrumentor_indirect_f_watch_data;

struct instrumentor_indirect_wf_watch_data
{
    struct kedr_coi_instrumentor_watch_data base;

    struct instrumentor_indirect_wf_ops_data* ops_data;
    /*
     *  Element for global map preventing reference to operations to be
     * instrumented by another instrumentor.
     */
    struct kedr_coi_hash_elem hash_elem_operations_field;
    // Local table element: reference to operations -> watch data
    struct kedr_coi_hash_elem hash_elem_ops_p;

    /*
     *  If this field is not NULL, it points to foreign watch data with
     * same reference to operations as this ("self-foreign" watching).
     */
    struct instrumentor_indirect_f_watch_data* foreign_watch_data;
};

struct instrumentor_indirect_f_watch_data
{
    struct kedr_coi_foreign_instrumentor_watch_data base;
    
    struct instrumentor_indirect_f_ops_data* ops_data;
    /*
     *  Element for global map preventing reference to operations to be
     * instrumented by another instrumentor.
     * 
     * Do not used in case of "self-foreign" watching.
     */
    struct kedr_coi_hash_elem hash_elem_operations_field;
    // Local table element: reference to operations -> watch data
    struct kedr_coi_hash_elem hash_elem_ops_p;
    /*
     *  If this field is not NULL, it points to normal watch data with
     * same reference to operations as this. ("self-foreign" watching).
     */
    struct instrumentor_indirect_wf_watch_data* binded_watch_data;
};


struct instrumentor_indirect_wf_impl
{
    struct kedr_coi_instrumentor_impl base;
    
    size_t operations_struct_size;
    const struct kedr_coi_replacement* replacements;
    
    struct kedr_coi_hash_table hash_table_ops_orig;
    struct kedr_coi_hash_table hash_table_ops_repl;
    struct kedr_coi_hash_table hash_table_ops_repl_foreign;

    // List of foreign instrumentors binded with this
    struct list_head foreign_instrumentor_impls;

    // Local table: reference to operations -> watch data
    struct kedr_coi_hash_table hash_table_ops_p;
    // Same but for foreign instrumentors
    struct kedr_coi_hash_table hash_table_ops_p_foreign;

    /*
     * Protect everything in normal instrumentor and its foreign "children":
     * 
     * -hash tables in instrumentors
     * -'binded_watch_data' and 'foreign_watch_data' in watch data structures
     * - refcounts of all types of ops_data
     * TODO: other protected elements
     */
    spinlock_t ops_lock;
};


struct instrumentor_indirect_f_impl
{
    struct kedr_coi_foreign_instrumentor_impl base;
    
    const struct kedr_coi_replacement* replacements;
    struct instrumentor_indirect_wf_impl* binded_instrumentor_impl;
    // List organization of foreign instrumentors
    struct list_head list;
};

static struct instrumentor_indirect_wf_ops_data*
instrumentor_indirect_wf_impl_find_ops_data_orig(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    const void* ops_orig)
{
    struct kedr_coi_hash_elem* hash_elem_ops_orig =
        kedr_coi_hash_table_find_elem(&instrumentor_impl->hash_table_ops_orig,
            ops_orig);
    
    return hash_elem_ops_orig
        ? container_of(
            hash_elem_ops_orig,
            struct instrumentor_indirect_wf_ops_data,
            hash_elem_ops_orig)
        : NULL;
}

static struct instrumentor_indirect_wf_ops_data*
instrumentor_indirect_wf_impl_find_ops_data(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    const void* ops_repl)
{
    struct kedr_coi_hash_elem* hash_elem_ops_repl =
        kedr_coi_hash_table_find_elem(&instrumentor_impl->hash_table_ops_repl,
            ops_repl);

    return hash_elem_ops_repl
        ? container_of(
            hash_elem_ops_repl,
            struct instrumentor_indirect_wf_ops_data,
            hash_elem_ops_repl)
        : NULL;
}

static struct instrumentor_indirect_f_ops_data*
instrumentor_indirect_wf_impl_find_foreign_ops_data(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    const void* ops_repl)
{
    struct kedr_coi_hash_elem* hash_elem_ops_repl =
        kedr_coi_hash_table_find_elem(&instrumentor_impl->hash_table_ops_repl_foreign,
            ops_repl);

    return hash_elem_ops_repl
        ? container_of(
            hash_elem_ops_repl,
            struct instrumentor_indirect_f_ops_data,
            hash_elem_ops_repl)
        : NULL;
}



static void
instrumentor_indirect_wf_impl_ref_ops_data(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    struct instrumentor_indirect_wf_ops_data* ops_data)
{
    ++ops_data->refs;
}

static void
instrumentor_indirect_wf_impl_unref_ops_data(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    struct instrumentor_indirect_wf_ops_data* ops_data)
{
    if(--ops_data->refs > 0) return;

    BUG_ON(!list_empty(&ops_data->foreign_ops_data_list));

    kedr_coi_hash_table_remove_elem(
        &instrumentor_impl->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig);

    kedr_coi_hash_table_remove_elem(
        &instrumentor_impl->hash_table_ops_repl,
        &ops_data->hash_elem_ops_repl);

    kfree(ops_data->ops_repl);
    kfree(ops_data);
}

static struct instrumentor_indirect_wf_ops_data*
instrumentor_indirect_wf_impl_add_ops_data(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    const void* ops_orig)
{
    struct instrumentor_indirect_wf_ops_data* ops_data =
        kmalloc(sizeof(*ops_data), GFP_ATOMIC);
    
    if(ops_data == NULL)
    {
        pr_err("Failed to allocate operations data structure for "
            "instrumentor with foreign support.");
        return NULL;
    }
    
    ops_data->ops_repl =
        kmalloc(instrumentor_impl->operations_struct_size, GFP_ATOMIC);
    
    if(ops_data->ops_repl == NULL)
    {
        pr_err("Failed to allocate replaced operations for operations "
            "data for instrumentor with foreign support.");
        goto ops_repl_err;
    }
    ops_data->ops_orig = ops_orig;
    
    kedr_coi_hash_elem_init(&ops_data->hash_elem_ops_orig, ops_orig);
    if(kedr_coi_hash_table_add_elem(
        &instrumentor_impl->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig))
    {
        goto elem_ops_orig_err;
    }
    
    kedr_coi_hash_elem_init(&ops_data->hash_elem_ops_repl, ops_data->ops_repl);
    if(kedr_coi_hash_table_add_elem(
        &instrumentor_impl->hash_table_ops_repl,
        &ops_data->hash_elem_ops_repl))
    {
        goto elem_ops_repl_err;
    }

    INIT_LIST_HEAD(&ops_data->foreign_ops_data_list);

    if(ops_orig)
    {
        memcpy(ops_data->ops_repl, ops_orig,
            instrumentor_impl->operations_struct_size);
    }
    else
    {
        memset(ops_data->ops_repl, 0,
            instrumentor_impl->operations_struct_size);
    }
    replace_operations(ops_data->ops_repl,
        instrumentor_impl->replacements);
    
    ops_data->refs = 1;
    return ops_data;

elem_ops_repl_err:
    kedr_coi_hash_table_remove_elem(&instrumentor_impl->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig);
elem_ops_orig_err:
    kfree(ops_data->ops_repl);
ops_repl_err:
    kfree(ops_data);
    
    return NULL;
}


static struct instrumentor_indirect_wf_ops_data*
instrumentor_indirect_wf_impl_get_ops_data(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    const void* ops)
{
    struct instrumentor_indirect_wf_ops_data* ops_data;
    struct instrumentor_indirect_f_ops_data* foreign_ops_data;
    
    ops_data = instrumentor_indirect_wf_impl_find_ops_data_orig(
        instrumentor_impl,
        ops);
    
    if(ops_data) goto ops_data_found;
    
    ops_data = instrumentor_indirect_wf_impl_find_ops_data(
        instrumentor_impl,
        ops);
    
    if(ops_data) goto ops_data_found;

    foreign_ops_data = instrumentor_indirect_wf_impl_find_foreign_ops_data(
        instrumentor_impl,
        ops);
    
    if(foreign_ops_data)
    {
        ops_data = foreign_ops_data->binded_ops_data;
        goto ops_data_found;
    }

    return instrumentor_indirect_wf_impl_add_ops_data(
        instrumentor_impl,
        ops);

ops_data_found:
    instrumentor_indirect_wf_impl_ref_ops_data(
        instrumentor_impl, ops_data);
    
    return ops_data;
}


static void
instrumentor_indirect_wf_impl_ref_foreign_ops_data(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    struct instrumentor_indirect_f_ops_data* foreign_ops_data)
{
    ++foreign_ops_data->refs;
}

static void
instrumentor_indirect_wf_impl_unref_foreign_ops_data(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    struct instrumentor_indirect_f_ops_data* foreign_ops_data)
{
    if(--foreign_ops_data->refs > 0) return;
    
    list_del(&foreign_ops_data->list);
    kedr_coi_hash_table_remove_elem(
        &instrumentor_impl->hash_table_ops_repl_foreign,
        &foreign_ops_data->hash_elem_ops_repl);
    
    instrumentor_indirect_wf_impl_unref_ops_data(
        instrumentor_impl, foreign_ops_data->binded_ops_data);
    
    kfree(foreign_ops_data->ops_repl);
    kfree(foreign_ops_data);
}


static struct instrumentor_indirect_f_ops_data*
instrumentor_indirect_f_impl_add_ops_data(
    struct instrumentor_indirect_f_impl* instrumentor_impl,
    const void* ops_orig,
    struct instrumentor_indirect_wf_ops_data* binded_ops_data_hint)
{
    struct instrumentor_indirect_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;

    struct instrumentor_indirect_f_ops_data* foreign_ops_data =
        kmalloc(sizeof(*foreign_ops_data), GFP_ATOMIC);
    
    if(foreign_ops_data == NULL)
    {
        pr_err("Failed to allocate foreign operations data structure.");
        return NULL;
    }
    
    foreign_ops_data->ops_repl = kmalloc(
        binded_instrumentor_impl->operations_struct_size, GFP_ATOMIC);
    
    if(foreign_ops_data->ops_repl == NULL)
    {
        pr_err("Failed to allocate replaced operations structure "
            "for foreign operations data.");
        goto ops_repl_err;
    }
    
    kedr_coi_hash_elem_init(&foreign_ops_data->hash_elem_ops_repl,
        foreign_ops_data->ops_repl);
    if(kedr_coi_hash_table_add_elem(
        &binded_instrumentor_impl->hash_table_ops_repl_foreign,
        &foreign_ops_data->hash_elem_ops_repl))
    {
        goto elem_ops_repl_err;
    }
    
    foreign_ops_data->instrumentor_impl = instrumentor_impl;
    
    if(binded_ops_data_hint && (binded_ops_data_hint->ops_orig == ops_orig))
    {
        instrumentor_indirect_wf_impl_ref_ops_data(
            binded_instrumentor_impl, binded_ops_data_hint);
        foreign_ops_data->binded_ops_data = binded_ops_data_hint;
    }
    else
    {
        foreign_ops_data->binded_ops_data =
            instrumentor_indirect_wf_impl_get_ops_data(
                binded_instrumentor_impl, ops_orig);
        if(foreign_ops_data->binded_ops_data == NULL) goto binded_ops_data_err;
    }
    
    list_add_tail(&foreign_ops_data->list,
        &foreign_ops_data->binded_ops_data->foreign_ops_data_list);
    /* 'ops_repl' cannot be NULL, so do not check them. */
    memcpy(foreign_ops_data->ops_repl,
        foreign_ops_data->binded_ops_data->ops_repl,
        binded_instrumentor_impl->operations_struct_size);
    replace_operations(foreign_ops_data->ops_repl,
        instrumentor_impl->replacements);
    
    foreign_ops_data->refs = 1;
    return foreign_ops_data;

binded_ops_data_err:
    kedr_coi_hash_table_remove_elem(
        &binded_instrumentor_impl->hash_table_ops_repl_foreign,
        &foreign_ops_data->hash_elem_ops_repl);
elem_ops_repl_err:
    kfree(foreign_ops_data->ops_repl);
ops_repl_err:
    kfree(foreign_ops_data);
    return NULL;
}

static struct instrumentor_indirect_f_ops_data*
instrumentor_indirect_f_impl_get_ops_data(
    struct instrumentor_indirect_f_impl* instrumentor_impl,
    const void* ops,
    struct instrumentor_indirect_wf_ops_data* binded_ops_data_hint)
{
    struct instrumentor_indirect_wf_ops_data* binded_ops_data;
    struct instrumentor_indirect_f_ops_data* foreign_ops_data;
    
    struct instrumentor_indirect_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;
    
    if(binded_ops_data_hint)
    {
        if((binded_ops_data_hint->ops_orig == ops)
            && (binded_ops_data_hint->ops_repl == ops))
        {
            binded_ops_data = binded_ops_data_hint;
            goto binded_ops_data_found;
        }
    }

    binded_ops_data = instrumentor_indirect_wf_impl_find_ops_data(
        binded_instrumentor_impl, ops);
    if(binded_ops_data) goto binded_ops_data_found;
    
    binded_ops_data = instrumentor_indirect_wf_impl_find_ops_data_orig(
        binded_instrumentor_impl, ops);
    if(binded_ops_data) goto binded_ops_data_found;

    foreign_ops_data = instrumentor_indirect_wf_impl_find_foreign_ops_data(
        binded_instrumentor_impl, ops);
    
    if(foreign_ops_data) goto foreign_ops_data_found;
    
    return instrumentor_indirect_f_impl_add_ops_data(
        instrumentor_impl,
        ops,
        NULL);

binded_ops_data_found:
    list_for_each_entry(foreign_ops_data, &binded_ops_data->foreign_ops_data_list, list)
    {
        if(foreign_ops_data->instrumentor_impl == instrumentor_impl)
        {
            goto foreign_ops_data_found;
        }
    }

    return instrumentor_indirect_f_impl_add_ops_data(
        instrumentor_impl,
        binded_ops_data->ops_orig,
        binded_ops_data);

foreign_ops_data_found:
    instrumentor_indirect_wf_impl_ref_foreign_ops_data(
        binded_instrumentor_impl, foreign_ops_data);
    return foreign_ops_data;
}

/*
 * Return operations struct corresponded to original operations.
 * At least, return same operations with garantee that given operation
 * in that struct is original.
 * 
 * On error return ERR_PTR().
 * 
 * Should be executed under lock.
 */
static const void* instrumentor_indirect_impl_get_orig_operations_nodata(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    const void* ops,
    size_t operation_offset)
{
    void* op;
    
    struct instrumentor_indirect_wf_ops_data* binded_ops_data;
    struct instrumentor_indirect_f_ops_data* foreign_ops_data =
        instrumentor_indirect_wf_impl_find_foreign_ops_data(
            instrumentor_impl, ops);

    if(foreign_ops_data != NULL)
        return foreign_ops_data->binded_ops_data->ops_orig;
    
    binded_ops_data = instrumentor_indirect_wf_impl_find_ops_data(
        instrumentor_impl, ops);
    
    if(binded_ops_data != NULL)
        return binded_ops_data->ops_orig;

    binded_ops_data = instrumentor_indirect_wf_impl_find_ops_data_orig(
        instrumentor_impl, ops);

    if(binded_ops_data)
        return ops;

    op = get_orig_operation_nodata(ops, operation_offset,
            instrumentor_impl->replacements);
        
    return IS_ERR(op) ? op : ops;
}


//**** Low-level API of indirect instrumentor as foreign ******//
static int instrumentor_indirect_f_impl_replace_operations(
    struct instrumentor_indirect_f_impl* instrumentor_impl,
    struct instrumentor_indirect_f_watch_data* watch_data_new,
    const void** ops_p)
{
    unsigned long flags;
    struct instrumentor_indirect_f_ops_data* ops_data;
    struct instrumentor_indirect_wf_watch_data* binded_watch_data;
    
    int result;
    
    struct instrumentor_indirect_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;

    spin_lock_irqsave(&binded_instrumentor_impl->ops_lock, flags);
    
    /* 
     * Check for collisions with another instrumentors.
     */

    kedr_coi_hash_elem_init(
        &watch_data_new->hash_elem_operations_field, ops_p);

    result = indirect_operations_fields_instrumented_add_unique(
        &watch_data_new->hash_elem_operations_field);
    
    if(result == 0)
    {
        binded_watch_data = NULL;
    }
    else if(result == -EBUSY)
    {
        struct kedr_coi_hash_elem* binded_watch_data_elem =
            kedr_coi_hash_table_find_elem(
                &binded_instrumentor_impl->hash_table_ops_p, ops_p);

        if(binded_watch_data_elem == NULL)
        {
            pr_err("Foreign watching for operations at %p failed because "
                "them are already watched by another interceptor.", ops_p);
            result = -EBUSY;
            goto err;
        }

        binded_watch_data = container_of(binded_watch_data_elem,
            typeof(*binded_watch_data), hash_elem_ops_p);
        
        if(binded_watch_data->foreign_watch_data)
        {
            pr_err("Foreign watching for operations at %p failed because "
                "them are already watched by another foreign interceptor.", ops_p);
            result = -EBUSY;
            goto err;
        }
    }
    else goto err;

    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_ops_p, ops_p);
    result = kedr_coi_hash_table_add_elem(
        &binded_instrumentor_impl->hash_table_ops_p_foreign,
        &watch_data_new->hash_elem_ops_p);
    if(result) goto hash_elem_ops_p_err;
    
    ops_data = instrumentor_indirect_f_impl_get_ops_data(
        instrumentor_impl,
        *ops_p,
        binded_watch_data
            ? binded_watch_data->ops_data
            : NULL);
    if(ops_data == NULL)
    {
        result = -ENOMEM;
        goto get_ops_data_err;
    }


    watch_data_new->ops_data = ops_data;
    *ops_p = ops_data->ops_repl;

    if(binded_watch_data)
    {
        binded_watch_data->foreign_watch_data = watch_data_new;
        watch_data_new->binded_watch_data = binded_watch_data;
        instrumentor_indirect_wf_impl_unref_ops_data(
            binded_instrumentor_impl, binded_watch_data->ops_data);
        instrumentor_indirect_wf_impl_ref_ops_data(
            binded_instrumentor_impl, ops_data->binded_ops_data);
        binded_watch_data->ops_data = ops_data->binded_ops_data;
    }
    else
    {
        watch_data_new->binded_watch_data = NULL;
    }
    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
    
    return 0;

get_ops_data_err:
    kedr_coi_hash_table_remove_elem(
        &binded_instrumentor_impl->hash_table_ops_p_foreign,
        &watch_data_new->hash_elem_ops_p);
hash_elem_ops_p_err:
    if(!binded_watch_data)indirect_operations_fields_instrumented_remove(
        &watch_data_new->hash_elem_operations_field);
err:
    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
    return result;
}

// Should be executed under lock
static int instrumentor_indirect_f_impl_update_operations_internal(
    struct instrumentor_indirect_f_impl* instrumentor_impl,
    struct instrumentor_indirect_f_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;
    struct instrumentor_indirect_f_ops_data* ops_data_new;
    
    if(*ops_p == watch_data->ops_data->ops_repl)
    {
        // up-to-date
        return 0;
    }
    
    if(*ops_p == watch_data->ops_data->binded_ops_data->ops_orig)
    {
        *ops_p = watch_data->ops_data->ops_repl;
        return 0;
    }

    ops_data_new = instrumentor_indirect_f_impl_get_ops_data(
        instrumentor_impl,
        *ops_p,
        NULL);
    if(ops_data_new == NULL) return -ENOMEM;

    instrumentor_indirect_wf_impl_unref_foreign_ops_data(
        binded_instrumentor_impl, watch_data->ops_data);
    watch_data->ops_data = ops_data_new;
    *ops_p = ops_data_new->ops_repl;

    if(watch_data->binded_watch_data)
    {
        struct instrumentor_indirect_wf_watch_data* binded_watch_data =
            watch_data->binded_watch_data;
        instrumentor_indirect_wf_impl_unref_ops_data(
            binded_instrumentor_impl,
            binded_watch_data->ops_data);
        instrumentor_indirect_wf_impl_ref_ops_data(
            binded_instrumentor_impl,
            ops_data_new->binded_ops_data);
        binded_watch_data->ops_data = ops_data_new->binded_ops_data;
    }
    
    return 0;
}
    
static void instrumentor_indirect_f_impl_clean_replacement(
    struct instrumentor_indirect_f_impl* instrumentor_impl,
    struct instrumentor_indirect_f_watch_data* watch_data,
    const void** ops_p)
{
    unsigned long flags;
    struct instrumentor_indirect_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;
    
    spin_lock_irqsave(&binded_instrumentor_impl->ops_lock, flags);
    if(ops_p && (*ops_p == watch_data->ops_data->ops_repl))
    {
        *ops_p = watch_data->binded_watch_data
            ? watch_data->binded_watch_data->ops_data->ops_repl
            : watch_data->ops_data->binded_ops_data->ops_orig;
    }

    instrumentor_indirect_wf_impl_unref_foreign_ops_data(
        binded_instrumentor_impl, watch_data->ops_data);
        
    kedr_coi_hash_table_remove_elem(
        &binded_instrumentor_impl->hash_table_ops_p_foreign,
        &watch_data->hash_elem_ops_p);
        
    if(watch_data->binded_watch_data == NULL)
    {
        indirect_operations_fields_instrumented_remove(
            &watch_data->hash_elem_operations_field);
    }
    else
    {
        watch_data->binded_watch_data->foreign_watch_data = NULL;
    }
    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
}

static void instrumentor_indirect_f_impl_destroy(
    struct instrumentor_indirect_f_impl* instrumentor_impl)
{
    list_del(&instrumentor_impl->list);

    kfree(instrumentor_impl);
}

//****Interface of the indirect instrumentor as foreign ****//
static struct kedr_coi_foreign_instrumentor_watch_data*
instrumentor_indirect_f_impl_iface_alloc_watch_data(
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl)
{
    struct instrumentor_indirect_f_watch_data* watch_data =
        kmalloc(sizeof(*watch_data), GFP_KERNEL);
    
    return watch_data ? &watch_data->base : NULL;
}

static void
instrumentor_indirect_f_impl_iface_free_watch_data(
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data)
{
    struct instrumentor_indirect_f_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    kfree(watch_data_real);
}

static int instrumentor_indirect_f_impl_iface_replace_operations(
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    struct instrumentor_indirect_f_impl* instrumentor_impl =
        container_of(iface_f_impl, typeof(*instrumentor_impl), base);

    struct instrumentor_indirect_f_watch_data* watch_data_new_real =
        container_of(watch_data_new, typeof(*watch_data_new_real), base);
    
    return instrumentor_indirect_f_impl_replace_operations(
        instrumentor_impl,
        watch_data_new_real,
        ops_p);
}

static int instrumentor_indirect_f_impl_iface_update_operations(
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    int result;
    unsigned long flags;
    
    struct instrumentor_indirect_f_impl* instrumentor_impl =
        container_of(iface_f_impl, typeof(*instrumentor_impl), base);

    struct instrumentor_indirect_f_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    struct instrumentor_indirect_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;
    
    spin_lock_irqsave(&binded_instrumentor_impl->ops_lock, flags);
    
    result = instrumentor_indirect_f_impl_update_operations_internal(
        instrumentor_impl,
        watch_data_real,
        ops_p);
    
    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
    
    return result;
}

static void instrumentor_indirect_f_impl_iface_clean_replacement(
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect_f_impl* instrumentor_impl =
        container_of(iface_f_impl, typeof(*instrumentor_impl), base);

    struct instrumentor_indirect_f_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    instrumentor_indirect_f_impl_clean_replacement(
        instrumentor_impl,
        watch_data_real,
        ops_p);
}


static int
instrumentor_indirect_f_impl_iface_restore_foreign_operations_nodata(
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    const void** ops_p,
    size_t operation_offset,
    void** op_orig)
{
    unsigned long flags;
    const void* ops_orig;
    
    struct instrumentor_indirect_f_impl* instrumentor_impl =
        container_of(iface_f_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;
    
    spin_lock_irqsave(&binded_instrumentor_impl->ops_lock, flags);
    
    ops_orig = instrumentor_indirect_impl_get_orig_operations_nodata(
        binded_instrumentor_impl,
        *ops_p,
        operation_offset);
    
    if(IS_ERR(ops_orig))
    {
        *op_orig = ERR_PTR(PTR_ERR(ops_orig));
    }
    else
    {
        *ops_p = ops_orig;
        *op_orig = operation_at_offset(ops_orig, operation_offset);
    }
    
    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
    
    return IS_ERR(*op_orig) ? PTR_ERR(*op_orig) : 0;
}

static int
instrumentor_indirect_f_impl_iface_restore_foreign_operations(
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* foreign_data,
    const void** ops_p,
    size_t operation_offset,
    void** op_orig)
{
    unsigned long flags;
    
    
    struct instrumentor_indirect_f_impl* instrumentor_impl =
        container_of(iface_f_impl, typeof(*instrumentor_impl), base);
    struct instrumentor_indirect_f_watch_data* foreign_data_real =
        container_of(foreign_data, typeof(*foreign_data_real), base);
    struct instrumentor_indirect_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;
    
    spin_lock_irqsave(&binded_instrumentor_impl->ops_lock, flags);
    
    if((*ops_p == foreign_data_real->ops_data->ops_repl)
        || (*ops_p == foreign_data_real->ops_data->binded_ops_data->ops_repl)
        || (*ops_p == foreign_data_real->ops_data->binded_ops_data->ops_orig))
    {
        *ops_p = foreign_data_real->ops_data->binded_ops_data->ops_orig;
        *op_orig = operation_at_offset(*ops_p, operation_offset);
    }
    else
    {
        const void* ops_orig = instrumentor_indirect_impl_get_orig_operations_nodata(
            binded_instrumentor_impl,
            *ops_p,
            operation_offset);
        if(IS_ERR(ops_orig))
        {
            *op_orig = ERR_PTR(PTR_ERR(ops_orig));
        }
        else
        {
            *ops_p = ops_orig;
            *op_orig = operation_at_offset(ops_orig, operation_offset);
        }
    }
    
    spin_unlock_irqrestore(&instrumentor_impl->binded_instrumentor_impl->ops_lock, flags);
    
    return IS_ERR(*op_orig) ? PTR_ERR(*op_orig) : 0;
}

static void instrumentor_indirect_f_impl_iface_destroy_impl(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl)
{
    struct instrumentor_indirect_f_impl* impl_real =
        container_of(iface_impl, typeof(*impl_real), base);
    
    instrumentor_indirect_f_impl_destroy(impl_real);
}

static struct kedr_coi_foreign_instrumentor_impl_iface
instrumentor_indirect_f_impl_iface =
{
    .alloc_watch_data = instrumentor_indirect_f_impl_iface_alloc_watch_data,
    .free_watch_data = instrumentor_indirect_f_impl_iface_free_watch_data,

    .replace_operations = instrumentor_indirect_f_impl_iface_replace_operations,
    .clean_replacement = instrumentor_indirect_f_impl_iface_clean_replacement,
    .update_operations = instrumentor_indirect_f_impl_iface_update_operations,
    
    .destroy_impl = instrumentor_indirect_f_impl_iface_destroy_impl,
    
    .restore_foreign_operations =
        instrumentor_indirect_f_impl_iface_restore_foreign_operations,
    .restore_foreign_operations_nodata =
        instrumentor_indirect_f_impl_iface_restore_foreign_operations_nodata
};

// Constructor(for internal use) of indirect foreign instrumentor implementation
static struct instrumentor_indirect_f_impl*
instrumentor_indirect_f_impl_create_disconnected(
    const struct kedr_coi_replacement* replacements)
{
    struct instrumentor_indirect_f_impl* instrumentor_impl =
        kmalloc(sizeof(*instrumentor_impl), GFP_KERNEL);
    
    if(instrumentor_impl == NULL)
    {
        pr_err("Failed to allocate structure for foreign instrumentor implementation");
        return NULL;
    }
    
    INIT_LIST_HEAD(&instrumentor_impl->list);
    instrumentor_impl->binded_instrumentor_impl = NULL;
    instrumentor_impl->replacements = replacements;

    instrumentor_impl->base.interface = &instrumentor_indirect_f_impl_iface;
    
    return instrumentor_impl;
}

//**** Low-level API of indirect instrumentor for foreign support******//
// Should be executed under lock
static int instrumentor_indirect_wf_impl_replace_operations_internal(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    struct instrumentor_indirect_wf_watch_data* watch_data_new,
    const void** ops_p,
    struct instrumentor_indirect_wf_ops_data* ops_data_hint)
{
    struct instrumentor_indirect_wf_ops_data* ops_data;
    struct instrumentor_indirect_f_watch_data* foreign_watch_data;
    
    int result;
    
    /* 
     * Check for collisions with another instrumentors.
     */

    kedr_coi_hash_elem_init(
        &watch_data_new->hash_elem_operations_field, ops_p);

    result = indirect_operations_fields_instrumented_add_unique(
        &watch_data_new->hash_elem_operations_field);
    
    if(result == 0)
    {
        foreign_watch_data = NULL;
    }
    else if(result == -EBUSY)
    {
        struct kedr_coi_hash_elem* foreign_watch_data_elem = 
            kedr_coi_hash_table_find_elem(
                &instrumentor_impl->hash_table_ops_p_foreign,
                ops_p);
        
        if(foreign_watch_data_elem == NULL)
        {
            pr_err("Indirect watching for operations at %p failed because "
                "them are already watched by another interceptor.", ops_p);
            goto err;
        }
        
        foreign_watch_data = container_of(foreign_watch_data_elem,
            typeof(*foreign_watch_data), hash_elem_ops_p);
    }
    else goto err;
    
    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_ops_p, ops_p);
    result = kedr_coi_hash_table_add_elem(
        &instrumentor_impl->hash_table_ops_p,
        &watch_data_new->hash_elem_ops_p);

    if(result) goto hash_elem_ops_p_err;
    
    if(foreign_watch_data)
    {
        struct instrumentor_indirect_f_ops_data* foreign_ops_data =
            foreign_watch_data->ops_data;
        if((*ops_p == foreign_ops_data->ops_repl)
            || (*ops_p == foreign_ops_data->binded_ops_data->ops_repl)
            || (*ops_p == foreign_ops_data->binded_ops_data->ops_orig));
        else
        {
            foreign_ops_data = instrumentor_indirect_f_impl_get_ops_data(
                foreign_ops_data->instrumentor_impl, *ops_p, NULL);
            if(foreign_ops_data == NULL)
            {
                result = -ENOMEM;
                goto get_ops_data_err;
            }
            instrumentor_indirect_wf_impl_unref_foreign_ops_data(
                instrumentor_impl, foreign_watch_data->ops_data);
            instrumentor_indirect_wf_impl_ref_foreign_ops_data(
                instrumentor_impl, foreign_ops_data);
            
            foreign_watch_data->ops_data = foreign_ops_data;
        }
        ops_data = foreign_ops_data->binded_ops_data;
        instrumentor_indirect_wf_impl_ref_ops_data(
            instrumentor_impl,
            ops_data);
    }
    else
    {
        ops_data = instrumentor_indirect_wf_impl_get_ops_data(
            instrumentor_impl, *ops_p);
        if(ops_data == NULL)
        {
            result = -ENOMEM;
            goto get_ops_data_err;
        }
    }
    
    watch_data_new->ops_data = ops_data;

    if(foreign_watch_data)
    {
        watch_data_new->foreign_watch_data = foreign_watch_data;
        foreign_watch_data->binded_watch_data = watch_data_new;
        
        indirect_operations_fields_instrumented_transmit(
            &foreign_watch_data->hash_elem_operations_field,
            &watch_data_new->hash_elem_operations_field);
            
        *ops_p = foreign_watch_data->ops_data->ops_repl;
    }
    else
    {
        watch_data_new->foreign_watch_data = NULL;
        
        *ops_p = ops_data->ops_repl;
    }

    return 0;

get_ops_data_err:
    kedr_coi_hash_table_remove_elem(
        &instrumentor_impl->hash_table_ops_p,
        &watch_data_new->hash_elem_ops_p);
hash_elem_ops_p_err:
    if(!foreign_watch_data)
        indirect_operations_fields_instrumented_remove(
            &watch_data_new->hash_elem_operations_field);
err:
    return result;
}

// Should be executed under lock.
static int instrumentor_indirect_wf_impl_update_operations_internal(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    struct instrumentor_indirect_wf_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect_wf_ops_data* ops_data_new;

    if(watch_data->foreign_watch_data)
    {
        // Replacement is "self-foreign"
        return instrumentor_indirect_f_impl_update_operations_internal(
            watch_data->foreign_watch_data->ops_data->instrumentor_impl,
            watch_data->foreign_watch_data,
            ops_p);
    }
    
    if(*ops_p == watch_data->ops_data->ops_repl)
    {
        // up-to-date
        return 0;
    }
    
    if(*ops_p == watch_data->ops_data->ops_orig)
    {
        *ops_p = watch_data->ops_data->ops_repl;
        return 0;
    }
    
     ops_data_new = instrumentor_indirect_wf_impl_get_ops_data(
        instrumentor_impl,
        *ops_p);
    if(ops_data_new == NULL) return -ENOMEM;

    instrumentor_indirect_wf_impl_unref_ops_data(
        instrumentor_impl, watch_data->ops_data);
    watch_data->ops_data = ops_data_new;
    *ops_p = ops_data_new->ops_repl;

    return 0;
}

static void instrumentor_indirect_wf_impl_clean_replacement(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    struct instrumentor_indirect_wf_watch_data* watch_data,
    const void** ops_p)
{
    unsigned long flags;
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);

    if(ops_p)
    {
        if(!watch_data->foreign_watch_data)
        {
            if(*ops_p == watch_data->ops_data->ops_repl)
                *ops_p = watch_data->ops_data->ops_orig;
        }
    }

    instrumentor_indirect_wf_impl_unref_ops_data(
        instrumentor_impl, watch_data->ops_data);
    
    kedr_coi_hash_table_remove_elem(
        &instrumentor_impl->hash_table_ops_p,
        &watch_data->hash_elem_ops_p);
    
    if(watch_data->foreign_watch_data)
    {
        watch_data->foreign_watch_data->binded_watch_data = NULL;
        indirect_operations_fields_instrumented_transmit(
            &watch_data->hash_elem_operations_field,
            &watch_data->foreign_watch_data->hash_elem_operations_field);
    }
    else
    {
        indirect_operations_fields_instrumented_remove(
            &watch_data->hash_elem_operations_field);
    }

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
}


static void instrumentor_indirect_wf_impl_destroy(
    struct instrumentor_indirect_wf_impl* instrumentor_impl)
{
    BUG_ON(!list_empty(&instrumentor_impl->foreign_instrumentor_impls));
    
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_orig, NULL);
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_repl, NULL);
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_repl_foreign, NULL);
    
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_p, NULL);
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_p_foreign, NULL);
    
    kfree(instrumentor_impl);
}


static struct instrumentor_indirect_f_impl*
instrumentor_indirect_impl_create_foreign_indirect(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    const struct kedr_coi_replacement* replacements)
{
    struct instrumentor_indirect_f_impl* foreign_instrumentor_impl =
        instrumentor_indirect_f_impl_create_disconnected(
            replacements);
    
    if(foreign_instrumentor_impl == NULL) return NULL;
    
    list_add_tail(&foreign_instrumentor_impl->list,
        &instrumentor_impl->foreign_instrumentor_impls);
    
    foreign_instrumentor_impl->binded_instrumentor_impl = instrumentor_impl;
    
    return foreign_instrumentor_impl;
}
    
static int instrumentor_indirect_impl_replace_operations_from_foreign(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    struct instrumentor_indirect_wf_watch_data* watch_data_new,
    const void** ops_p,
    struct instrumentor_indirect_f_watch_data* foreign_data)
{
    int result;
    
    unsigned long flags;
    
    struct instrumentor_indirect_f_ops_data* ops_data =
        foreign_data->ops_data;
    struct instrumentor_indirect_wf_ops_data* binded_ops_data;
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);

    ops_data = foreign_data->ops_data;
    binded_ops_data = ops_data->binded_ops_data;
    
    if((*ops_p == ops_data->ops_repl)
        || (*ops_p == binded_ops_data->ops_repl))
    {
        *ops_p = binded_ops_data->ops_orig;
        goto restored;
    }
    if(*ops_p == binded_ops_data->ops_orig) goto restored;

    ops_data = instrumentor_indirect_wf_impl_find_foreign_ops_data(
        instrumentor_impl,
        *ops_p);
    if(ops_data)
    {
        binded_ops_data = ops_data->binded_ops_data;
        *ops_p = binded_ops_data->ops_orig;
        goto restored;
    }
    
    binded_ops_data = instrumentor_indirect_wf_impl_find_ops_data(
        instrumentor_impl,
        *ops_p);
    if(binded_ops_data)
    {
        *ops_p = binded_ops_data->ops_orig;
        goto restored;
    }
    
restored:
    result = instrumentor_indirect_wf_impl_replace_operations_internal(
        instrumentor_impl,
        watch_data_new,
        ops_p,
        binded_ops_data);
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    return result;
}

static int instrumentor_indirect_impl_update_operations_from_foreign(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    struct instrumentor_indirect_wf_watch_data* watch_data,
    const void** ops_p,
    struct instrumentor_indirect_f_watch_data* foreign_data)
{
    int result;
    
    unsigned long flags;
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);

    result = instrumentor_indirect_wf_impl_update_operations_internal(
        instrumentor_impl,
        watch_data,
        ops_p);
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    return result;
}

static int instrumentor_indirect_impl_chain_operation(
    struct instrumentor_indirect_wf_impl* instrumentor_impl,
    struct instrumentor_indirect_f_impl* foreign_instrumentor_impl,
    const void** ops_p,
    size_t operation_offset,
    void** op_chained)
{
    int result;
    unsigned long flags;
    const void* ops_orig;
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    ops_orig = instrumentor_indirect_impl_get_orig_operations_nodata(
        instrumentor_impl, *ops_p, operation_offset);
    
    if(IS_ERR(ops_orig))
    {
        *op_chained = ERR_PTR(PTR_ERR(ops_orig));
        result = PTR_ERR(ops_orig);
    }
    else
    {
        *ops_p = ops_orig;
        *op_chained = operation_at_offset(*ops_p, operation_offset);
        result = 0;
    }
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        
    return result;
}


//****Interface of the indirect instrumentor with foreign support****//
static struct kedr_coi_instrumentor_watch_data*
instrumentor_indirect_wf_impl_iface_alloc_watch_data(
    struct kedr_coi_instrumentor_impl* iface_wf_impl)
{
    struct instrumentor_indirect_wf_watch_data* watch_data =
        kmalloc(sizeof(*watch_data), GFP_KERNEL);
    
    return watch_data ? &watch_data->base : NULL;
}

static void
instrumentor_indirect_wf_impl_iface_free_watch_data(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    struct instrumentor_indirect_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    kfree(watch_data_real);
}

static int
instrumentor_indirect_wf_impl_iface_replace_operations(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    unsigned long flags;
    int result;
    
    struct instrumentor_indirect_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect_wf_watch_data* watch_data_new_real =
        container_of(watch_data_new, typeof(*watch_data_new_real), base);

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    result = instrumentor_indirect_wf_impl_replace_operations_internal(
        instrumentor_impl,
        watch_data_new_real,
        ops_p,
        NULL);
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return result;
}

static int
instrumentor_indirect_wf_impl_iface_update_operations(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    unsigned long flags;
    int result;

    struct instrumentor_indirect_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    result = instrumentor_indirect_wf_impl_update_operations_internal(
        instrumentor_impl,
        watch_data_real,
        ops_p);
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return result;
}


static void
instrumentor_indirect_wf_impl_iface_clean_replacement(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);

    instrumentor_indirect_wf_impl_clean_replacement(
        instrumentor_impl,
        watch_data_real,
        ops_p);
}

static void*
instrumentor_indirect_wf_impl_iface_get_orig_operation(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void* ops,
    size_t operation_offset)
{
    unsigned long flags;
    void* op;
    const void* ops_orig;
    
    struct instrumentor_indirect_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    if(ops == watch_data_real->ops_data->ops_repl)
    {
        op = operation_at_offset(watch_data_real->ops_data->ops_orig,
            operation_offset);
        goto out;
    }
    if(watch_data_real->foreign_watch_data)
    {
        struct instrumentor_indirect_f_ops_data* foreign_ops_data =
            watch_data_real->foreign_watch_data->ops_data;
        if(ops == foreign_ops_data->ops_repl)
        {
            op = operation_at_offset(
                foreign_ops_data->binded_ops_data->ops_orig, 
                operation_offset);
            goto out;
        }
    }
    if(ops == watch_data_real->ops_data->ops_orig)
    {
        op = operation_at_offset(watch_data_real->ops_data->ops_orig,
            operation_offset);
        goto out;
    }

    
    ops_orig = instrumentor_indirect_impl_get_orig_operations_nodata(
        instrumentor_impl, ops, operation_offset);
    if(IS_ERR(ops_orig))
    {
        op = ERR_PTR(PTR_ERR(ops_orig));
    }
    else
    {
        op = operation_at_offset(ops_orig, operation_offset);
    }
out:    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return op;
}

static void*
instrumentor_indirect_wf_impl_iface_get_orig_operation_nodata(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    const void* ops,
    size_t operation_offset)
{
    unsigned long flags;
    void* op;
    const void* ops_orig;
    
    struct instrumentor_indirect_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);

    ops_orig = instrumentor_indirect_impl_get_orig_operations_nodata(
        instrumentor_impl, ops, operation_offset);
    
    if(IS_ERR(ops_orig))
    {
        op = ERR_PTR(PTR_ERR(ops_orig));
    }
    else
    {
        op = operation_at_offset(ops_orig, operation_offset);
    }

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return op;
}


static struct kedr_coi_foreign_instrumentor_impl*
instrumentor_indirect_wf_impl_iface_foreign_instrumentor_impl_create(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    const struct kedr_coi_replacement* replacements)
{
    struct instrumentor_indirect_f_impl* foreign_instrumentor_impl;
    struct instrumentor_indirect_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);
    
    foreign_instrumentor_impl =
        instrumentor_indirect_impl_create_foreign_indirect(
            instrumentor_impl,
            replacements);
    
    if(foreign_instrumentor_impl == NULL) return NULL;
    
    return &foreign_instrumentor_impl->base;
}

static int instrumentor_indirect_wf_impl_iface_replace_operations_from_foreign(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data_new,
    const void** ops_p,
    struct kedr_coi_foreign_instrumentor_watch_data* foreign_data,
    size_t operation_offset,
    void** op_repl)
{
    int result;

    struct instrumentor_indirect_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);

    struct instrumentor_indirect_wf_watch_data* watch_data_new_real =
        container_of(watch_data_new, typeof(*watch_data_new_real), base);
    struct instrumentor_indirect_f_watch_data* foreign_data_real =
        container_of(foreign_data, typeof(*foreign_data_real), base);

    result = instrumentor_indirect_impl_replace_operations_from_foreign(
        instrumentor_impl,
        watch_data_new_real,
        ops_p,
        foreign_data_real);
    
    *op_repl = operation_at_offset(*ops_p, operation_offset);
    
    return result;
}

static int instrumentor_indirect_wf_impl_iface_update_operations_from_foreign(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p,
    struct kedr_coi_foreign_instrumentor_watch_data* foreign_data,
    size_t operation_offset,
    void** op_repl)
{
    int result;

    struct instrumentor_indirect_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);

    struct instrumentor_indirect_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    struct instrumentor_indirect_f_watch_data* foreign_data_real =
        container_of(foreign_data, typeof(*foreign_data_real), base);

    result = instrumentor_indirect_impl_update_operations_from_foreign(
        instrumentor_impl,
        watch_data_real,
        ops_p,
        foreign_data_real);
    
    *op_repl = operation_at_offset(*ops_p, operation_offset);
    
    return result;
}

static int instrumentor_indirect_wf_impl_iface_chain_operation(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p,
    size_t operation_offset,
    void** op_chained)
{
    int result;

    struct instrumentor_indirect_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);

    struct instrumentor_indirect_f_impl* foreign_instrumentor_impl =
        container_of(iface_f_impl, typeof(*foreign_instrumentor_impl), base);

    result = instrumentor_indirect_impl_chain_operation(
        instrumentor_impl,
        foreign_instrumentor_impl,
        ops_p,
        operation_offset,
        op_chained);
    
    return result;
}


static void instrumentor_indirect_wf_impl_iface_destroy_impl(
    struct kedr_coi_instrumentor_impl* iface_wf_impl)
{
    struct instrumentor_indirect_wf_impl* impl_real =
        container_of(iface_wf_impl, typeof(*impl_real), base);
    
    instrumentor_indirect_wf_impl_destroy(impl_real);
}

static struct kedr_coi_instrumentor_with_foreign_impl_iface
instrumentor_indirect_wf_impl_iface =
{
    .base =
    {
        .alloc_watch_data = instrumentor_indirect_wf_impl_iface_alloc_watch_data,
        .free_watch_data = instrumentor_indirect_wf_impl_iface_free_watch_data,

        .replace_operations = instrumentor_indirect_wf_impl_iface_replace_operations,
        .clean_replacement = instrumentor_indirect_wf_impl_iface_clean_replacement,
        .update_operations = instrumentor_indirect_wf_impl_iface_update_operations,
        
        .destroy_impl = instrumentor_indirect_wf_impl_iface_destroy_impl,
        
        .get_orig_operation =
            instrumentor_indirect_wf_impl_iface_get_orig_operation,
        .get_orig_operation_nodata =
            instrumentor_indirect_wf_impl_iface_get_orig_operation_nodata
    },
    
    .foreign_instrumentor_impl_create =
        instrumentor_indirect_wf_impl_iface_foreign_instrumentor_impl_create,
    .replace_operations_from_foreign =
        instrumentor_indirect_wf_impl_iface_replace_operations_from_foreign,
    .update_operations_from_foreign =
        instrumentor_indirect_wf_impl_iface_update_operations_from_foreign,
    .chain_operation = instrumentor_indirect_wf_impl_iface_chain_operation
};

// Constructor for indirect instrumentor with foreign support
static struct kedr_coi_instrumentor_with_foreign*
instrumentor_indirect_wf_create(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_instrumentor_with_foreign* instrumentor;
    
    int result;

    struct instrumentor_indirect_wf_impl* instrumentor_impl;
    
    if((replacements == NULL) || (replacements[0].operation_offset == -1))
    {
        return instrumentor_with_foreign_empty_create();
    }
    
    instrumentor_impl = kmalloc(sizeof(*instrumentor_impl), GFP_KERNEL);
    
    if(instrumentor_impl == NULL)
    {
        pr_err("Failed to allocate structure for indirect instrumentor "
            "implementation with foreign support");
        return NULL;
    }
    
    result = kedr_coi_hash_table_init(&instrumentor_impl->hash_table_ops_orig);
    if(result) goto hash_table_ops_orig_err;
    
    result = kedr_coi_hash_table_init(&instrumentor_impl->hash_table_ops_repl);
    if(result) goto hash_table_ops_repl_err;
    
    result = kedr_coi_hash_table_init(&instrumentor_impl->hash_table_ops_repl_foreign);
    if(result) goto hash_table_ops_repl_foreign_err;
    
    result = kedr_coi_hash_table_init(&instrumentor_impl->hash_table_ops_p);
    if(result) goto hash_table_ops_p_err;
    
    result = kedr_coi_hash_table_init(&instrumentor_impl->hash_table_ops_p_foreign);
    if(result) goto hash_table_ops_p_foreign_err;

    instrumentor_impl->operations_struct_size = operations_struct_size;
    instrumentor_impl->replacements = replacements;
    spin_lock_init(&instrumentor_impl->ops_lock);
    INIT_LIST_HEAD(&instrumentor_impl->foreign_instrumentor_impls);
    
    instrumentor_impl->base.interface = &instrumentor_indirect_wf_impl_iface.base;
    
    instrumentor = kedr_coi_instrumentor_with_foreign_create(&instrumentor_impl->base);
    
    if(instrumentor == NULL) goto instrumentor_err;

    return instrumentor;

instrumentor_err:
    kedr_coi_hash_table_destroy(
        &instrumentor_impl->hash_table_ops_p_foreign, NULL);
hash_table_ops_p_foreign_err:
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_p,
        NULL);
hash_table_ops_p_err:
    kedr_coi_hash_table_destroy(
        &instrumentor_impl->hash_table_ops_repl_foreign, NULL);
hash_table_ops_repl_foreign_err:
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_repl,
        NULL);
hash_table_ops_repl_err:
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_orig,
        NULL);
hash_table_ops_orig_err:
    kfree(instrumentor_impl);
    
    return NULL;
}

/**********************************************************************/
/************Advanced instrumentor for indirect operation**************/
/**********************************************************************/
/*
 * Implementation of indirect instrumentor with replacing operations
 * inside structure instead of replacing whole operations structure.
 * 
 * NULL pointer to operations is processed specially: it is replaced
 * with interceptor-global pointer to zeroed operations, and then replaced.
 */

/*
 * Global map of operations structures which is changed by
 * interception mechanizm.
 * 
 * Need to prevent interceptors badly affect on one another.
 */

/*
 * Add pointer to the operations struct(as key) to the map of
 * instrumented operations.
 * 
 * If field already in the map, return -EBUSY.
 */

static int indirect_operations_instrumented_add_unique(
    struct kedr_coi_hash_elem* hash_elem_operations);

static void indirect_operations_instrumented_remove(
    struct kedr_coi_hash_elem* hash_elem_operations);


/*
 * Per-operations data for instrumentor.
 * 
 * These data contain all information about replaced operations,
 * 
 * Per-operations data may be searched for by operations.
 * 
 * Per-object data share per-operations data for same operations struct.
 * 
 * Note that after initializing content of the instance is constant(final)
 * except 'refs' field and 'hash_elem_ops'.
 */
struct instrumentor_indirect1_ops_data
{
    // hash table organization in instrumentor implementation
    struct kedr_coi_hash_elem hash_elem_ops;
    
    void* ops;// Pointer to the operations

    void* ops_orig;// Allocated operations structure with original operations.
    
    int refs;
    // Used for global map of indirect operations
    struct kedr_coi_hash_elem hash_elem_operations;
};

/*
 *  Watch data for instrumentor.
 * 
 * Contain reference to per-operations data plus key for global map
 * of instrumented operations fields.
 */
struct instrumentor_indirect1_watch_data
{
    struct kedr_coi_instrumentor_watch_data base;
    
    struct instrumentor_indirect1_ops_data* ops_data;
    
    struct kedr_coi_hash_elem hash_elem_operations_field;
};

/*
 * Implementation of advanced indirect instrumentor.
 */
struct instrumentor_indirect1_impl
{
    struct kedr_coi_instrumentor_impl base;
    
    struct kedr_coi_hash_table hash_table_ops;
    /*
     * Protect table of per-operations data from concurrent access.
     * Also protect reference count of per-operations data instance
     * from concurrent access.
     */
    spinlock_t ops_lock;
    
    size_t operations_struct_size;
    
    const struct kedr_coi_replacement* replacements;
    /* 
     * Operations which are used when original pointer is NULL.
     * 
     * NOTE: Field is used only in interface functions. All other
     * functions should never see NULL pointer to operations.
     */
    void* null_ops;
};

static void* instrumentor_indirect1_impl_get_null_ops(
    struct instrumentor_indirect1_impl* instrumentor_impl)
{
    /* TODO: lazy initialization. */
    return instrumentor_impl->null_ops;
}

/*
 * Initialize instance of implementation of
 * advanced indirect instrumentor.
 */
static int instrumentor_indirect1_impl_init(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result = kedr_coi_hash_table_init(&instrumentor_impl->hash_table_ops);
    if(result)
    {
        pr_err("Failed to initialize hash table for operations");
        return result;
    }
    
    instrumentor_impl->null_ops = kzalloc(
        operations_struct_size, GFP_KERNEL);
    if(instrumentor_impl->null_ops == NULL)
    {
        pr_err("Failed to allocate null-operations structure.");
        kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops, NULL);
        return -ENOMEM;
    }
    
    instrumentor_impl->operations_struct_size = operations_struct_size;
    
    instrumentor_impl->replacements = replacements;
    
    spin_lock_init(&instrumentor_impl->ops_lock);
    
    return 0;
}

/*
 * Destroy instance of advance indirect instrumentor.
 * (Instance inself is not freed.)
 */
static void instrumentor_indirect1_impl_finalize(
    struct instrumentor_indirect1_impl* instrumentor_impl)
{
    kfree(instrumentor_impl->null_ops);
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops, NULL);
}

/*
 * Search per-operations data.
 * 
 * Should be executed under lock.
 */
static struct instrumentor_indirect1_ops_data*
instrumentor_indirect1_impl_find_ops_data(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    const void* ops)
{
    struct kedr_coi_hash_elem* hash_elem_ops =
        kedr_coi_hash_table_find_elem(
            &instrumentor_impl->hash_table_ops,
            ops);
    if(hash_elem_ops == NULL) return NULL;
    return container_of(hash_elem_ops, struct instrumentor_indirect1_ops_data, hash_elem_ops);
}

/*
 * Allocate instance of per-operations data, fill its fields and
 * add to the hash tables of operations(instrumentor-local and global ones).
 * 
 * NOTE: This function also replace operations in 'ops'.
 * 
 * Should be executed under lock.
 */

static struct instrumentor_indirect1_ops_data*
instrumentor_indirect1_impl_add_ops_data(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    void* ops)
{
    int result;

    struct instrumentor_indirect1_ops_data* ops_data =
        kmalloc(sizeof(*ops_data), GFP_ATOMIC);
    
    if(ops_data == NULL)
    {
        pr_err("Failed to allocate operations data for indirect instrumentor.");
        return NULL;
    }
    
    ops_data->ops_orig = kmalloc(
        instrumentor_impl->operations_struct_size, GFP_ATOMIC);
    if(ops_data->ops_orig == NULL)
    {
        pr_err("Failed to allocate buffer of origin operations for "
            "operations data for indirect instrumentor.");
        kfree(ops_data);
        return NULL;

    }
    
    ops_data->refs = 1;
    
    kedr_coi_hash_elem_init(&ops_data->hash_elem_operations, ops);
    
    result = indirect_operations_instrumented_add_unique(
        &ops_data->hash_elem_operations);
    switch(result)
    {
    case 0:
        break;
    case -EBUSY:
        pr_err("Indirect watching for operations %p failed because them "
            "are already watched by another interceptor.", ops);
    default:
        kfree(ops_data->ops_orig);
        kfree(ops_data);
        return NULL;
    }
    
    kedr_coi_hash_elem_init(&ops_data->hash_elem_ops, ops);
    
    kedr_coi_hash_table_add_elem(&instrumentor_impl->hash_table_ops,
        &ops_data->hash_elem_ops);
    
    ops_data->ops = ops;
    
    if(ops)
    {
        memcpy(ops_data->ops_orig, ops,
            instrumentor_impl->operations_struct_size);
    }
    else
    {
        /* 
         * NULL pointer to operations structure is equvalent to 
         * zeroes in all operations.
         */
        memset(ops_data->ops_orig, 0,
            instrumentor_impl->operations_struct_size);
    }
    
    replace_operations(ops, instrumentor_impl->replacements);
    
    return ops_data;
}

/*
 * Destroy content of instance of per-operation data
 * and free instance itself.
 * 
 * Should be executed under lock.
 */
static void
instrumentor_indirect1_impl_remove_ops_data(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    struct instrumentor_indirect1_ops_data* ops_data)
{
    restore_operations(ops_data->ops,
        instrumentor_impl->replacements,
        ops_data->ops_orig);

    kedr_coi_hash_table_remove_elem(&instrumentor_impl->hash_table_ops,
        &ops_data->hash_elem_ops);

    indirect_operations_instrumented_remove(&ops_data->hash_elem_operations);

    kfree(ops_data->ops_orig);
    kfree(ops_data);
}

/*
 * Increment reference count on per-operations data.
 * 
 * Should be executed under lock.
 */
static void instrumentor_indirect1_impl_ref_ops_data(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    struct instrumentor_indirect1_ops_data* ops_data)
{
    ops_data->refs++;
}
/*
 * Decrement reference count on per-operations data.
 * If it become 0, remove data.
 * 
 * Should be executed under lock.
 */
static void instrumentor_indirect1_impl_unref_ops_data(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    struct instrumentor_indirect1_ops_data* ops_data)
{
    if(--ops_data->refs > 0) return;
    
    instrumentor_indirect1_impl_remove_ops_data(instrumentor_impl, ops_data);
}


/*
 * Return per-operations data which corresponds to the given operations
 * and 'ref' these data.
 * 
 * If such data don't exist, create new one and return it.
 * 
 * Should be executed under lock.
 */

static struct instrumentor_indirect1_ops_data*
instrumentor_indirect1_impl_get_ops_data(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    void* ops)
{
    struct instrumentor_indirect1_ops_data* ops_data;
    
    // Look for existed per-operations data for that operations
    ops_data = instrumentor_indirect1_impl_find_ops_data(
        instrumentor_impl,
        ops);
    
    if(ops_data)
    {
        instrumentor_indirect1_impl_ref_ops_data(
            instrumentor_impl, ops_data);
        return ops_data;
    }

    ops_data = instrumentor_indirect1_impl_add_ops_data(
        instrumentor_impl, ops);

    if(ops_data == NULL) return NULL;
    
    return ops_data;
}

//*********Low-level API of advanced indirect instrumentor************//

static int instrumentor_indirect1_impl_replace_operations(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    struct instrumentor_indirect1_watch_data* watch_data_new,
    const void** ops_p)
{
    unsigned long flags;
    int result;
    struct instrumentor_indirect1_ops_data* ops_data;
    
    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_operations_field,
        ops_p);
    
    result = indirect_operations_fields_instrumented_add_unique(
        &watch_data_new->hash_elem_operations_field);
    
    switch(result)
    {
    case 0:
        break;
    case -EBUSY:
        pr_err("Indirect watching for operations at %p failed because "
            "them are already watched by another interceptor.", ops_p);
    default:
        return result;
    }

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    ops_data = instrumentor_indirect1_impl_get_ops_data(
        instrumentor_impl,
        (void*)*ops_p);
    
    if(ops_data == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        indirect_operations_fields_instrumented_remove(
            &watch_data_new->hash_elem_operations_field);
        return -ENOMEM;
    }
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);

    watch_data_new->ops_data = ops_data;
    return 0;
}

static int instrumentor_indirect1_impl_update_operations(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    struct instrumentor_indirect1_watch_data* watch_data,
    const void** ops_p)
{
    unsigned long flags;
    struct instrumentor_indirect1_ops_data* ops_data;
    struct instrumentor_indirect1_ops_data* ops_data_new;
    
    ops_data = watch_data->ops_data;
    
    if(*ops_p == ops_data->ops)
        return 0;

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    ops_data_new = instrumentor_indirect1_impl_get_ops_data(
        instrumentor_impl,
        (void*)*ops_p);
    
    if(ops_data_new == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        return -ENOMEM;
    }
    
    watch_data->ops_data = ops_data_new;
    instrumentor_indirect1_impl_unref_ops_data(instrumentor_impl, ops_data);

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return 0;
}

static void instrumentor_indirect1_impl_clean_replacement(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    struct instrumentor_indirect1_watch_data* watch_data,
    const void** ops_p)
{
    unsigned long flags;
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    instrumentor_indirect1_impl_unref_ops_data(instrumentor_impl,
        watch_data->ops_data);
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    indirect_operations_fields_instrumented_remove(
        &watch_data->hash_elem_operations_field);
}

static void* instrumentor_indirect1_impl_get_orig_operation_nodata(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    const void* ops,
    size_t operation_offset)
{
    unsigned long flags;
    void* op_orig;
    struct instrumentor_indirect1_ops_data* ops_data;
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    ops_data = instrumentor_indirect1_impl_find_ops_data(
        instrumentor_impl, ops);
    
    op_orig = ops_data
        ? operation_at_offset(ops_data->ops_orig, operation_offset)
        : get_orig_operation_nodata(ops, operation_offset,
            instrumentor_impl->replacements);
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return op_orig;
}


//************Interface of advance indirect instrumentor***************//
static struct kedr_coi_instrumentor_watch_data*
instrumentor_indirect1_impl_iface_alloc_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    struct instrumentor_indirect1_watch_data* watch_data =
        kmalloc(sizeof(*watch_data), GFP_KERNEL);
    if(watch_data == NULL) return NULL;
    
    return &watch_data->base;
}

static void
instrumentor_indirect1_impl_iface_free_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    struct instrumentor_indirect1_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    kfree(watch_data_real);
}


static int instrumentor_indirect1_impl_iface_replace_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    struct instrumentor_indirect1_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_watch_data* watch_data_new_real =
        container_of(watch_data_new, typeof(*watch_data_new_real), base);
    
    if(*ops_p == NULL)
    {
        /* NULL pointer to operations requires special processing */
        int result;
        /* Replace pointer to operations to predefined one */
        *ops_p = instrumentor_indirect1_impl_get_null_ops(instrumentor_impl);
        result = instrumentor_indirect1_impl_replace_operations(
            instrumentor_impl,
            watch_data_new_real,
            ops_p);
        if(result)
        {
            /* Rollback in case of error */
            *ops_p = NULL;
            return result;
        }
        return 0;
    }
    else
    {
        return instrumentor_indirect1_impl_replace_operations(
            instrumentor_impl,
            watch_data_new_real,
            ops_p);
    }
}

static int instrumentor_indirect1_impl_iface_update_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect1_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    if(*ops_p == NULL)
    {
        /* NULL pointer to operations requires special processing */
        int result;
        *ops_p = instrumentor_indirect1_impl_get_null_ops(instrumentor_impl);
        result = instrumentor_indirect1_impl_update_operations(
            instrumentor_impl,
            watch_data_real,
            ops_p);
        
        if(result)
        {
            /* Rollback in case of error */
            *ops_p = NULL;
            return result;
        }
        return 0;
    }
    else
    {
        return instrumentor_indirect1_impl_update_operations(
            instrumentor_impl,
            watch_data_real,
            ops_p);
    }
}

static void instrumentor_indirect1_impl_iface_clean_replacement(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect1_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    instrumentor_indirect1_impl_clean_replacement(
        instrumentor_impl,
        watch_data_real,
        ops_p);
    
    if(ops_p && (*ops_p == instrumentor_indirect1_impl_get_null_ops(instrumentor_impl)))
        *ops_p = NULL;
}



static void instrumentor_indirect1_impl_iface_destroy_impl(
    struct kedr_coi_instrumentor_impl* in_iface_impl)
{
    struct instrumentor_indirect1_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);

    instrumentor_indirect1_impl_finalize(instrumentor_impl);

    kfree(instrumentor_impl);
}

static void* instrumentor_indirect1_impl_iface_get_orig_operation(
    struct kedr_coi_instrumentor_impl* in_iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void* ops,
    size_t operation_offset)
{
    struct instrumentor_indirect1_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);

    return operation_at_offset(watch_data_real->ops_data->ops_orig,
        operation_offset);
}
    

static void* instrumentor_indirect1_impl_iface_get_orig_operation_nodata(
    struct kedr_coi_instrumentor_impl* in_iface_impl,
    const void* ops,
    size_t operation_offset)
{
    struct instrumentor_indirect1_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_indirect1_impl_get_orig_operation_nodata(
        instrumentor_impl,
        ops,
        operation_offset);
}

    
static struct kedr_coi_instrumentor_impl_iface
instrumentor_indirect1_impl_iface =
{
    .alloc_watch_data = instrumentor_indirect1_impl_iface_alloc_watch_data,
    .free_watch_data = instrumentor_indirect1_impl_iface_free_watch_data,
    
    .replace_operations = instrumentor_indirect1_impl_iface_replace_operations,
    .clean_replacement = instrumentor_indirect1_impl_iface_clean_replacement,
    .update_operations = instrumentor_indirect1_impl_iface_update_operations,
    
    .destroy_impl = instrumentor_indirect1_impl_iface_destroy_impl,
    
    .get_orig_operation = instrumentor_indirect1_impl_iface_get_orig_operation,
    .get_orig_operation_nodata = instrumentor_indirect1_impl_iface_get_orig_operation_nodata
};

// Constructor for advanced indirect instrumentor
struct kedr_coi_instrumentor*
instrumentor_indirect1_create(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_instrumentor* instrumentor;
    
    int result;

    struct instrumentor_indirect1_impl* instrumentor_impl;
    
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
    
    result = instrumentor_indirect1_impl_init(instrumentor_impl,
        operations_struct_size,
        replacements);
    if(result)
    {
        kfree(instrumentor_impl);
        return NULL;
    }

    instrumentor_impl->base.interface = &instrumentor_indirect1_impl_iface;
    
    instrumentor = kedr_coi_instrumentor_create(&instrumentor_impl->base);
    
    if(instrumentor == NULL)
    {
        instrumentor_indirect1_impl_finalize(instrumentor_impl);
        kfree(instrumentor_impl);
        return NULL;
    }

    return instrumentor;
}

//******** Advanced indirect instrumentor with foreign support*********//
/*
 * Per-operations data for advanced indirect instrumentor with
 * foreign support.
 * 
 * Aside other fields, this data contain head of the list of
 * per-operations data for foreign instrumentors, which share these
 * operations with that data.
 */
struct instrumentor_indirect1_wf_ops_data
{
    /*
     *  Current object operations.
     * 
     * After instance creation, this pointer is constant, but operations
     * themselves are not. Only atomic changing of one operation is
     * garanteed (this is need for calling operations from the kernel.)
     */
    void* ops;
    /*
     * Copy of original object operations.
     * 
     * After instance creation this pointer and its content are constant
     * (final).
     */
    void* ops_orig;
    // Refcount for share per-operation instance between per-object data.
    int refs;
    /*
     * List of foreign per-operations data, which share same operations.
     * From the most inner one to the outmost, which operations are
     * stored(indirectly) in the object.
     */
    struct list_head foreign_ops_data;
    // Element in the global table of instrumented operations.
    struct kedr_coi_hash_elem hash_elem_operations;
    // Element in the instrumentor's table of per-operations data.
    struct kedr_coi_hash_elem hash_elem_ops;
};

/*
 * Per-operations data of advanced indirect foreign instrumentor.
 * 
 * These data do not contain operations itself, instead them contain
 * pointer to the per-operations structure of the binded instrumentor.
 * 
 * But them contain pointer to replacements of owner. This is useful for
 * perform rollback of the total operations when these per-operations
 * data are removed.
 * 
 * 'binded_ops_data' and 'replacements' fields are final after 
 * instance of per-operations data is created.
 */
struct instrumentor_indirect1_f_ops_data
{
    // Refcount for share these data inside foreign instrumentor.
    int refs;
    /*
     *  Element in the foreing per-operation data list in the normal
     * per-operations data, which connected with this operations.
     */
    struct list_head list;
    // Pointer to the normal per-operations data.
    struct instrumentor_indirect1_wf_ops_data* binded_ops_data;
    // Foreign instrumentor to which these data belong.
    struct instrumentor_indirect1_f_impl* instrumentor_impl;
};

/*
 *  Watch data of the advance indirect instrumentor
 * with foreign support.
 */
struct instrumentor_indirect1_wf_watch_data
{
    struct kedr_coi_instrumentor_watch_data base;
    
    struct instrumentor_indirect1_wf_ops_data* ops_data;
    
    struct kedr_coi_hash_elem hash_elem_operations_field;
    /*
     * If not NULL, points to foreign watch data for the same reference 
     * to operations(self-foreign watch).
     */
    struct instrumentor_indirect1_f_watch_data* foreign_watch_data;
    // Local map: reference to operations -> watch data
    struct kedr_coi_hash_elem hash_elem_ops_p;
};

// Watch data of the advance indirect foreign instrumentor.
struct instrumentor_indirect1_f_watch_data
{
    struct kedr_coi_foreign_instrumentor_watch_data base;
    
    struct instrumentor_indirect1_f_ops_data* ops_data;
    
    struct kedr_coi_hash_elem hash_elem_operations_field;
    /*
     * If not NULL, points to normal watch data for the same reference 
     * to operations(self-foreign watch).
     */
    struct instrumentor_indirect1_wf_watch_data* binded_watch_data;
    // Local map: reference to operations -> watch data
    struct kedr_coi_hash_elem hash_elem_ops_p;
};

/*
 * Implementation of the advance indirect instrumentor
 * with foreign support.
 */
struct instrumentor_indirect1_wf_impl
{
    struct kedr_coi_instrumentor_impl base;
    
    struct kedr_coi_hash_table hash_table_ops;
    
    struct kedr_coi_hash_table hash_table_ops_p;
    struct kedr_coi_hash_table hash_table_ops_p_foreign;
    /*
     * Protect table of per-operations data from concurrent access.
     * Protect refcount of per-operations data from concurrent access.
     * Protect list of foreign per-operations data within normal
     * per-operations data from concurrent access.
     * Protect tables with reference to operations.
     * Protect foreign per-operations data.
     */
    spinlock_t ops_lock;
    
    size_t operations_struct_size;
    
    const struct kedr_coi_replacement* replacements;
    /* 
     * Operations which are used when original pointer is NULL.
     * 
     * NOTE: Field is used only in interface functions. All other
     * functions should never see NULL pointer to operations.
     */
    void* null_ops;
};

static void* instrumentor_indirect1_wf_impl_get_null_ops(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl)
{
    return instrumentor_impl->null_ops;
}

// Initialize instance of the instrumentor.
static int instrumentor_indirect1_wf_impl_init(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result = kedr_coi_hash_table_init(
        &instrumentor_impl->hash_table_ops);
    if(result) goto hash_table_ops_err;
    
    result = kedr_coi_hash_table_init(
        &instrumentor_impl->hash_table_ops_p);
    if(result) goto hash_table_ops_p_err;
    
    result = kedr_coi_hash_table_init(
        &instrumentor_impl->hash_table_ops_p_foreign);
    if(result) goto hash_table_ops_p_foreign_err;

    instrumentor_impl->null_ops = kzalloc(operations_struct_size,
        GFP_KERNEL);
    if(instrumentor_impl->null_ops == NULL)
    {
        pr_err("Failed to allocate zeroed operations for replace null-pointer.");
        result = -ENOMEM;
        goto null_ops_err;
    }
    
    instrumentor_impl->operations_struct_size = operations_struct_size;

    instrumentor_impl->replacements = replacements;
    
    spin_lock_init(&instrumentor_impl->ops_lock);
    
    return 0;

null_ops_err:
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops, NULL);
hash_table_ops_p_foreign_err:
    kedr_coi_hash_table_destroy(
        &instrumentor_impl->hash_table_ops_p_foreign, NULL);
hash_table_ops_p_err:
    kedr_coi_hash_table_destroy(
        &instrumentor_impl->hash_table_ops, NULL);
hash_table_ops_err:
    return result;
}

// Destroy instance of the instrumentor.
// (Instance itself does not freed.)
static void instrumentor_indirect1_wf_impl_finalize(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl)
{
    kfree(instrumentor_impl->null_ops);
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops, NULL);
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_p, NULL);
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_p_foreign, NULL);
}

/*
 *  Search per-operations data for given operations.
 * 
 * Should be executed with lock taken.
 */
static struct instrumentor_indirect1_wf_ops_data*
instrumentor_indirect1_wf_impl_find_ops_data(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    const void* ops)
{
    struct kedr_coi_hash_elem* ops_data_elem =
        kedr_coi_hash_table_find_elem(
            &instrumentor_impl->hash_table_ops,
            ops);
    
    return ops_data_elem
        ? container_of(ops_data_elem,
                    struct instrumentor_indirect1_wf_ops_data,
                    hash_elem_ops)
        : NULL;
}

/*
 * Allocate instance of per-operations data, fill its fields and
 * add to the hash tables of operations(instrumentor-local and global ones).
 * 
 * NOTE: This function also replace operations in 'ops'.
 * 
 * Should be executed with lock taken.
 */

static struct instrumentor_indirect1_wf_ops_data*
instrumentor_indirect1_wf_impl_add_ops_data(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    void* ops)
{
    int result;

    struct instrumentor_indirect1_wf_ops_data* ops_data =
        kmalloc(sizeof(*ops_data), GFP_ATOMIC);
    
    if(ops_data == NULL)
    {
        pr_err("Failed to allocate operations data for instrumentor with foreign support.");
        return NULL;
    }
    
    ops_data->ops_orig = kmalloc(
        instrumentor_impl->operations_struct_size, GFP_ATOMIC);
    if(ops_data->ops_orig == NULL)
    {
        pr_err("Failed to allocate buffer for original operations.");
        kfree(ops_data);
        return NULL;
    }
    
    ops_data->refs = 1;
    INIT_LIST_HEAD(&ops_data->foreign_ops_data);
    
    kedr_coi_hash_elem_init(&ops_data->hash_elem_operations, ops);
    
    result = indirect_operations_instrumented_add_unique(
        &ops_data->hash_elem_operations);
    if(result)
    {
        if(result == -EBUSY)
        {
            pr_err("Indirect watching for operations %p failed because "
                "them are already watched by another interceptor.", ops);
        }
        kfree(ops_data->ops_orig);
        kfree(ops_data);
        return NULL;
    }
    
    kedr_coi_hash_elem_init(&ops_data->hash_elem_ops, ops);
    
    kedr_coi_hash_table_add_elem(&instrumentor_impl->hash_table_ops,
        &ops_data->hash_elem_ops);
    
    ops_data->ops = ops;
    
    memcpy(ops_data->ops_orig, ops,
        instrumentor_impl->operations_struct_size);
    
    replace_operations(ops, instrumentor_impl->replacements);

    return ops_data;
}


/*
 * Increment reference count of per-operations data.
 * 
 * Should be executed with lock taken.
 */
static void
instrumentor_indirect1_wf_impl_ref_ops_data(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    struct instrumentor_indirect1_wf_ops_data* ops_data)
{
    ops_data->refs++;
}

/*
 * Decrement reference count of per-operations data.
 * If it become 0, remove these data.
 * 
 * Should be executed with lock taken.
 */
static void
instrumentor_indirect1_wf_impl_unref_ops_data(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    struct instrumentor_indirect1_wf_ops_data* ops_data)
{
    if(--ops_data->refs > 0) return;

    restore_operations(ops_data->ops, instrumentor_impl->replacements,
        ops_data->ops_orig);

    kedr_coi_hash_table_remove_elem(&instrumentor_impl->hash_table_ops,
        &ops_data->hash_elem_ops);

    indirect_operations_instrumented_remove(&ops_data->hash_elem_operations);

    kfree(ops_data->ops_orig);
    kfree(ops_data);
}

/*
 * Return per-operations data which corresponds to the given operations
 * and 'ref' these data.
 * 
 * If such data don't exist, create new one and return it.
 * 
 * Should be executed with lock taken.
 */
static struct instrumentor_indirect1_wf_ops_data*
instrumentor_indirect1_wf_impl_get_ops_data(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    void* ops)
{
    struct instrumentor_indirect1_wf_ops_data* ops_data;
    
    ops_data = instrumentor_indirect1_wf_impl_find_ops_data(
        instrumentor_impl,
        ops);
    
    if(ops_data != NULL)
    {
        instrumentor_indirect1_wf_impl_ref_ops_data(
            instrumentor_impl,
            ops_data);
        
        return ops_data;
    }

    return instrumentor_indirect1_wf_impl_add_ops_data(
            instrumentor_impl,
            ops);
}

/*
 * Internal function with multiple users.
 * 
 * Return operation of given object, which is a replacement of the NORMAL
 * instrumentor (not foreign one(s)!).
 * If operations with given offset is not replaced by normal instrumentor,
 * return object's original operation.
 * 
 * NOTE: Always success.
 */
static void*
instrumentor_indirect1_wf_impl_get_repl_operation_common(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    struct instrumentor_indirect1_wf_ops_data* ops_data,
    size_t operation_offset)
{
    const struct kedr_coi_replacement* replacement;
    void* op_orig = operation_at_offset(ops_data->ops_orig,
        operation_offset);

    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        if(replacement->operation_offset == operation_offset)
        {
            return get_repl_operation(replacement, op_orig);
        }
    }
    
    return op_orig;
}

// Implementation of the advance indirect foreign instrumentor.
struct instrumentor_indirect1_f_impl
{
    struct kedr_coi_foreign_instrumentor_impl base;
    
    struct kedr_coi_hash_table hash_table_ops;
    
    const struct kedr_coi_replacement* replacements;
    
    struct instrumentor_indirect1_wf_impl* binded_instrumentor_impl;
};

/*
 *  Initialize instance of advanced indirect foreign instrumentor
 * implementation.
 */
static int instrumentor_indirect1_f_impl_init(
    struct instrumentor_indirect1_f_impl* instrumentor_impl,
    struct instrumentor_indirect1_wf_impl* binded_instrumentor_impl,
    const struct kedr_coi_replacement* replacements)
{
    int result = kedr_coi_hash_table_init(
        &instrumentor_impl->hash_table_ops);
    
    if(result) return result;

    instrumentor_impl->replacements = replacements;
    instrumentor_impl->binded_instrumentor_impl = binded_instrumentor_impl;
    
    return 0;
}

/*
 *  Destroy instance of advanced indirect foreign instrumentor
 * implementation. (Instance itself do not freed.)
 */
static void instrumentor_indirect1_f_impl_finalize(
    struct instrumentor_indirect1_f_impl* instrumentor_impl)
{
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops, NULL);
}

/*
 * Search foreign per-operations data according to binded ops data.
 * 
 * Should be executed with lock taken.
 */
static struct instrumentor_indirect1_f_ops_data*
instrumentor_indirect1_f_impl_find_ops_data(
    struct instrumentor_indirect1_f_impl* instrumentor_impl,
    struct instrumentor_indirect1_wf_ops_data* binded_ops_data)
{
    struct instrumentor_indirect1_f_ops_data* ops_data;
    list_for_each_entry(ops_data, &binded_ops_data->foreign_ops_data, list)
    {
        if(ops_data->instrumentor_impl == instrumentor_impl) return ops_data;
    }
    return NULL;
}

/*
 * Allocate instance of per-operations data, fill its fields and
 * add to the hash table of operations(instrumentor-local).
 * 
 * 'binded_ops_data' should be referenced before this function call.
 * 
 * NOTE: This function also replace operations in 'ops'.
 * 
 * Should be executed with lock taken.
 */
static struct instrumentor_indirect1_f_ops_data*
instrumentor_indirect1_f_impl_add_ops_data(
    struct instrumentor_indirect1_f_impl* instrumentor_impl,
    struct instrumentor_indirect1_wf_ops_data* binded_ops_data)
{
    const struct kedr_coi_replacement* replacement;

    struct instrumentor_indirect1_f_ops_data* ops_data;

    ops_data = kmalloc(sizeof(*ops_data), GFP_ATOMIC);
    
    if(ops_data == NULL)
    {
        pr_err("Failed to allocate operations data for foreign instrumentor.");
        return NULL;
    }
    
    INIT_LIST_HEAD(&ops_data->list);
    ops_data->instrumentor_impl = instrumentor_impl;
    ops_data->refs = 1;
    
    list_add_tail(&ops_data->list, &binded_ops_data->foreign_ops_data);
    ops_data->binded_ops_data = binded_ops_data;
    
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        // For foreign intstrumentor all replacements are external.
        BUG_ON(replacement->mode != replace_all);
        
        *operation_at_offset_p(binded_ops_data->ops, replacement->operation_offset) =
            replacement->repl;
    }
    
    return ops_data;
}

/*
 * Helper for implement 'remove' for foreign per-operation data.
 * 
 * Rollback given replacement after removing per-operation data,
 * container it, from the list.
 * 
 * 'binded_ops_data' are normal per-operations data corresponded to
 * deleted one.
 * 
 * 'ops_data_next' is the foreign per-operations data which comes after
 * deleted one in the list of 'binded_ops_data'.
 * 
 * Should be executed with lock taken.
 */
static void instrumentor_indirect1_f_impl_rollback_replacement(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    const struct kedr_coi_replacement* replacement,
    struct instrumentor_indirect1_wf_ops_data* binded_ops_data,
    struct instrumentor_indirect1_f_ops_data* ops_data_next)
{
    void** op_p = operation_at_offset_p(binded_ops_data->ops,
        replacement->operation_offset);

    BUG_ON(replacement->mode != replace_all);
    if(*op_p != replacement->repl) return;

    list_for_each_entry_continue_reverse(ops_data_next,
        &binded_ops_data->foreign_ops_data, list)
    {
        const struct kedr_coi_replacement* replacement_tmp;
        for(replacement_tmp = ops_data_next->instrumentor_impl->replacements;
            replacement_tmp->operation_offset != -1;
            replacement_tmp++)
        {
            if(replacement_tmp->operation_offset == replacement->operation_offset)
            {
                BUG_ON(replacement_tmp->mode != replace_all);
                *op_p = replacement_tmp->repl;
                return;
            }
        }
    }
    *op_p = instrumentor_indirect1_wf_impl_get_repl_operation_common(
        instrumentor_impl,
        binded_ops_data,
        replacement->operation_offset);
}

/*
 * Destroy instance of foreign per-operations data and
 * free instance itself.
 * 
 * Also unref normal per-operations data binded with removing one.
 *
 * Should be executed with lock taken.
 */
static void
instrumentor_indirect1_f_impl_remove_ops_data(
    struct instrumentor_indirect1_f_impl* instrumentor_impl,
    struct instrumentor_indirect1_f_ops_data* ops_data)
{
    struct instrumentor_indirect1_wf_ops_data* binded_ops_data =
        ops_data->binded_ops_data;
    const struct kedr_coi_replacement* replacement;
    struct instrumentor_indirect1_f_ops_data* ops_data_next =
        list_entry(ops_data->list.next, typeof(*ops_data), list);

    list_del(&ops_data->list);
    
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        instrumentor_indirect1_f_impl_rollback_replacement(
            instrumentor_impl->binded_instrumentor_impl,
            replacement,
            binded_ops_data,
            ops_data_next);
    }
       
    kfree(ops_data);
    
    instrumentor_indirect1_wf_impl_unref_ops_data(
        instrumentor_impl->binded_instrumentor_impl,
        binded_ops_data);
}

/*
 *  Increment reference count of foreign per-operations data.
 * 
 * Should be executed with lock taken.
 */

static void
instrumentor_indirect1_f_impl_ref_ops_data(
    struct instrumentor_indirect1_f_impl* instrumentor_impl,
    struct instrumentor_indirect1_f_ops_data* ops_data)
{
    ops_data->refs++;
}

/*
 * Decrement reference count of foreign per-operations data.
 * 
 * If it become 0, remove them.
 * 
 * Should be executed with locks taken.
 */
static void
instrumentor_indirect1_f_impl_unref_ops_data(
    struct instrumentor_indirect1_f_impl* instrumentor_impl,
    struct instrumentor_indirect1_f_ops_data* ops_data)
{
    if(--ops_data->refs > 0) return;

    instrumentor_indirect1_f_impl_remove_ops_data(
        instrumentor_impl,
        ops_data);
}

/*
 * Return per-operations data which corresponds to the given operations
 * and 'ref' these data.
 * 
 * If such data don't exist, create new one and return it.
 * 
 * Operations data for binded instrumentor may be passed as hint.
 *
 * Should be executed with lock.
 */
static struct instrumentor_indirect1_f_ops_data*
instrumentor_indirect1_f_impl_get_ops_data(
    struct instrumentor_indirect1_f_impl* instrumentor_impl,
    void* ops,
    struct instrumentor_indirect1_wf_ops_data* binded_ops_data_hint)
{
    struct instrumentor_indirect1_f_ops_data* ops_data;
    struct instrumentor_indirect1_wf_ops_data* binded_ops_data;
    
    if(binded_ops_data_hint && (binded_ops_data_hint->ops == ops))
    {
        binded_ops_data = binded_ops_data_hint;
    }
    else
    {
        binded_ops_data = instrumentor_indirect1_wf_impl_find_ops_data(
            instrumentor_impl->binded_instrumentor_impl, ops);
    }
    if(binded_ops_data)
    {
        ops_data = instrumentor_indirect1_f_impl_find_ops_data(
            instrumentor_impl, binded_ops_data);
        if(ops_data)
        {
            instrumentor_indirect1_f_impl_ref_ops_data(
                instrumentor_impl, ops_data);
            
            return ops_data;
        }
        instrumentor_indirect1_wf_impl_ref_ops_data(
            instrumentor_impl->binded_instrumentor_impl,
            binded_ops_data);
    }
    else
    {
        binded_ops_data = instrumentor_indirect1_wf_impl_add_ops_data(
            instrumentor_impl->binded_instrumentor_impl,
            ops);
        if(binded_ops_data == NULL) return NULL;
    }
    
    ops_data = instrumentor_indirect1_f_impl_add_ops_data(
        instrumentor_impl,
        binded_ops_data);
    
    if(ops_data == NULL)
    {
        instrumentor_indirect1_wf_impl_unref_ops_data(
            instrumentor_impl->binded_instrumentor_impl,
            binded_ops_data);
        
        return NULL;
    }

    return ops_data;
}

//**** Low level API of advanced foreign indirect instrumentor ****//
static int instrumentor_indirect1_f_impl_replace_operations(
    struct instrumentor_indirect1_f_impl* instrumentor_impl,
    struct instrumentor_indirect1_f_watch_data* watch_data_new,
    const void** ops_p)
{
    unsigned long flags;
    
    int result;
    struct instrumentor_indirect1_f_ops_data* ops_data;
    struct instrumentor_indirect1_wf_ops_data* binded_ops_data;
    struct instrumentor_indirect1_wf_watch_data* binded_watch_data;
    
    struct instrumentor_indirect1_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;
    
    spin_lock_irqsave(&binded_instrumentor_impl->ops_lock, flags);

    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_operations_field,
        ops_p);
    
    result = indirect_operations_fields_instrumented_add_unique(
        &watch_data_new->hash_elem_operations_field);
    
    if(result == 0)
    {
        binded_watch_data = NULL;
    }
    else if(result == -EBUSY)
    {
        struct kedr_coi_hash_elem* hash_elem_ops_p =
            kedr_coi_hash_table_find_elem(
                &binded_instrumentor_impl->hash_table_ops_p,
                ops_p);
        if(hash_elem_ops_p == NULL)
        {
            pr_err("Foreign watching for operations at %p failed because "
                "them are already watched by another interceptor.", ops_p);
            goto elem_operations_field_err;
        }
        binded_watch_data = container_of(hash_elem_ops_p,
            typeof(*binded_watch_data), hash_elem_ops_p);
        
        if(binded_watch_data->foreign_watch_data)
        {
            pr_err("Foreign watching for operations at %p failed because "
                "them are already watched by another foreign interceptor.", ops_p);
            goto elem_operations_field_err;
        }
    }
    else
    {
        goto elem_operations_field_err;
    }
    
    if(binded_watch_data && (binded_watch_data->ops_data->ops == *ops_p))
        binded_ops_data = binded_watch_data->ops_data;
    else
        binded_ops_data = NULL;

    ops_data = instrumentor_indirect1_f_impl_get_ops_data(
        instrumentor_impl,
        (void*)*ops_p,
        binded_ops_data);
    
    if(ops_data == NULL)
    {
        result = - ENOMEM;
        goto ops_data_err;
    }

    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_ops_p, ops_p);
    result = kedr_coi_hash_table_add_elem(
        &binded_instrumentor_impl->hash_table_ops_p_foreign,
        &watch_data_new->hash_elem_ops_p);
    if(result) goto hash_elem_ops_p_err;
    
    
    watch_data_new->ops_data = ops_data;
    
    if(binded_watch_data)
    {
        watch_data_new->binded_watch_data = binded_watch_data;
        binded_watch_data->foreign_watch_data = watch_data_new;

        if(binded_watch_data->ops_data != binded_ops_data)
        {
            instrumentor_indirect1_wf_impl_unref_ops_data(
                binded_instrumentor_impl,
                binded_watch_data->ops_data);
            instrumentor_indirect1_wf_impl_ref_ops_data(
                binded_instrumentor_impl,
                binded_ops_data);
            binded_watch_data->ops_data = binded_ops_data;
        }
    }
    else
    {
        watch_data_new->binded_watch_data = NULL;
    }

    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
    
    return 0;

hash_elem_ops_p_err:
    instrumentor_indirect1_f_impl_unref_ops_data(
        instrumentor_impl,
        ops_data);
ops_data_err:
    if(!binded_watch_data)
        indirect_operations_fields_instrumented_remove(
            &watch_data_new->hash_elem_operations_field);
elem_operations_field_err:
    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);

    return result;
}

static int instrumentor_indirect1_f_impl_update_operations(
    struct instrumentor_indirect1_f_impl* instrumentor_impl,
    struct instrumentor_indirect1_f_watch_data* watch_data,
    const void** ops_p)
{
    unsigned long flags;

    struct instrumentor_indirect1_f_ops_data* ops_data;
    struct instrumentor_indirect1_f_ops_data* ops_data_new;
    
    struct instrumentor_indirect1_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;
    
    ops_data = watch_data->ops_data;
    
    if(*ops_p == ops_data->binded_ops_data->ops)
        return 0;

    spin_lock_irqsave(&binded_instrumentor_impl->ops_lock, flags);
    
    ops_data_new = instrumentor_indirect1_f_impl_get_ops_data(
        instrumentor_impl,
        (void*)*ops_p,
        NULL);
    
    if(ops_data_new == NULL) goto ops_data_new_err;

    instrumentor_indirect1_f_impl_unref_ops_data(instrumentor_impl,
        ops_data);
    
    watch_data->ops_data = ops_data_new;
    if(watch_data->binded_watch_data)
    {
        instrumentor_indirect1_wf_impl_unref_ops_data(
            binded_instrumentor_impl,
            watch_data->binded_watch_data->ops_data);
        instrumentor_indirect1_wf_impl_ref_ops_data(
            binded_instrumentor_impl,
            watch_data->ops_data->binded_ops_data);
        watch_data->binded_watch_data->ops_data =
            watch_data->ops_data->binded_ops_data;
    }

    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
    
    return 0;

ops_data_new_err:
    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
    return -ENOMEM;
}


static void instrumentor_indirect1_f_impl_clean_replacement(
    struct instrumentor_indirect1_f_impl* instrumentor_impl,
    struct instrumentor_indirect1_f_watch_data* watch_data,
    const void** ops_p)
{
    unsigned long flags;
    
    struct instrumentor_indirect1_wf_impl* binded_instrumentor_impl =
        instrumentor_impl->binded_instrumentor_impl;

    spin_lock_irqsave(&binded_instrumentor_impl->ops_lock, flags);

    kedr_coi_hash_table_remove_elem(
        &binded_instrumentor_impl->hash_table_ops_p_foreign,
        &watch_data->hash_elem_ops_p);
    
    instrumentor_indirect1_f_impl_unref_ops_data(instrumentor_impl,
        watch_data->ops_data);
    
    if(watch_data->binded_watch_data)
    {
        watch_data->binded_watch_data = NULL;
    }
    else
    {
        indirect_operations_fields_instrumented_remove(
            &watch_data->hash_elem_operations_field);
    }
    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
}

// Should be executed under lock
static void*
instrumentor_indirect1_wf_impl_get_orig_operation_nodata_internal(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    const void* ops,
    size_t operation_offset)
{
    struct instrumentor_indirect1_wf_ops_data* ops_data;

    ops_data = instrumentor_indirect1_wf_impl_find_ops_data(
        instrumentor_impl,
        ops);
    
    if(ops_data != NULL)
    {
        return operation_at_offset(ops_data->ops_orig,
            operation_offset);
    }
    
    return get_orig_operation_nodata(ops, operation_offset,
        instrumentor_impl->replacements);
}

//*Low level API of advanced indirect instrumentor with foreign support*//

/*
 * Same as standard 'replace_operations' but also accept hint, which
 * is probably per-operations data corresponded to the given object.
 * 
 * Should be executed under lock.
 */
static int instrumentor_indirect1_wf_impl_replace_operations_internal(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    struct instrumentor_indirect1_wf_watch_data* watch_data_new,
    const void** ops_p,
    struct instrumentor_indirect1_wf_ops_data* ops_data_hint)
{
    int result;
    struct instrumentor_indirect1_wf_ops_data* ops_data;
    struct instrumentor_indirect1_f_watch_data* foreign_watch_data;
    
    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_operations_field,
        ops_p);
    
    result = indirect_operations_fields_instrumented_add_unique(
        &watch_data_new->hash_elem_operations_field);
    
    if(result == 0)
    {
        foreign_watch_data = NULL;
    }
    else if(result == -EBUSY)
    {
        struct kedr_coi_hash_elem* hash_elem_ops_p =
            kedr_coi_hash_table_find_elem(
                &instrumentor_impl->hash_table_ops_p_foreign,
                ops_p);
        if(hash_elem_ops_p == NULL)
        {
            pr_err("Indirect watching for operations at %p failed because "
                "them are already watched by another interceptor.", ops_p);
            goto elem_operations_field_err;
        }
        foreign_watch_data = container_of(hash_elem_ops_p,
            typeof(*foreign_watch_data),
            hash_elem_ops_p);
    }
    else
    {
        goto elem_operations_field_err;
    }

    if(ops_data_hint && (ops_data_hint->ops == *ops_p))
    {
        ops_data = ops_data_hint;
        instrumentor_indirect1_wf_impl_ref_ops_data(
            instrumentor_impl,
            ops_data);
    }
    else if(foreign_watch_data
        && (foreign_watch_data->ops_data->binded_ops_data->ops == *ops_p))
    {
        ops_data = foreign_watch_data->ops_data->binded_ops_data;
        instrumentor_indirect1_wf_impl_ref_ops_data(
            instrumentor_impl,
            ops_data);
    }
    else
    {
        ops_data = instrumentor_indirect1_wf_impl_get_ops_data(
            instrumentor_impl,
            (void*)*ops_p);

        if(ops_data == NULL)
        {
            result = -EINVAL;
            goto ops_data_err;
        }
    }

    kedr_coi_hash_elem_init(&watch_data_new->hash_elem_ops_p, ops_p);
    result = kedr_coi_hash_table_add_elem(
        &instrumentor_impl->hash_table_ops_p,
        &watch_data_new->hash_elem_ops_p);
    if(result) goto hash_elem_ops_p_err;
    
    if(foreign_watch_data)
    {
        if(foreign_watch_data->ops_data->binded_ops_data != ops_data)
        {
            struct instrumentor_indirect1_f_ops_data* foreign_ops_data =
                instrumentor_indirect1_f_impl_get_ops_data(
                    foreign_watch_data->ops_data->instrumentor_impl,
                    (void*)*ops_p,
                    ops_data);
            if(foreign_ops_data == NULL)
            {
                result = -ENOMEM;
                goto foreign_ops_data_err;
            }
            instrumentor_indirect1_f_impl_unref_ops_data(
                foreign_ops_data->instrumentor_impl,
                foreign_watch_data->ops_data);
            instrumentor_indirect1_f_impl_ref_ops_data(
                foreign_ops_data->instrumentor_impl,
                foreign_ops_data);
            foreign_watch_data->ops_data = foreign_ops_data;
        }
        watch_data_new->foreign_watch_data = foreign_watch_data;
        foreign_watch_data->binded_watch_data = watch_data_new;
        indirect_operations_fields_instrumented_transmit(
            &foreign_watch_data->hash_elem_operations_field,
            &watch_data_new->hash_elem_operations_field);
    }
    else
    {
        watch_data_new->foreign_watch_data = NULL;
    }
    
    watch_data_new->ops_data = ops_data;

    return 0;

foreign_ops_data_err:
    kedr_coi_hash_table_remove_elem(
        &instrumentor_impl->hash_table_ops_p,
        &watch_data_new->hash_elem_ops_p);
hash_elem_ops_p_err:
    instrumentor_indirect1_wf_impl_unref_ops_data(
        instrumentor_impl,
        ops_data);
ops_data_err:
    if(!foreign_watch_data)
        indirect_operations_fields_instrumented_remove(
            &watch_data_new->hash_elem_operations_field);
elem_operations_field_err:
    return result;
}

// Should be executed under lock
static int instrumentor_indirect1_wf_impl_update_operations_internal(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    struct instrumentor_indirect1_wf_watch_data* watch_data,
    const void** ops_p,
    struct instrumentor_indirect1_wf_ops_data* ops_data_hint)
{
    struct instrumentor_indirect1_wf_ops_data* ops_data;
    
    ops_data = watch_data->ops_data;
    
    if(*ops_p == ops_data->ops)
        return 0;

    if(ops_data_hint && (ops_data_hint->ops == *ops_p))
    {
        ops_data = ops_data_hint;
        instrumentor_indirect1_wf_impl_ref_ops_data(
            instrumentor_impl,
            ops_data);
    }
    else
    {
        ops_data = instrumentor_indirect1_wf_impl_get_ops_data(
            instrumentor_impl,
            (void*)*ops_p);
        
        if(ops_data == NULL)
            return -EINVAL;
    }

    if(watch_data->foreign_watch_data)
    {
        struct instrumentor_indirect1_f_watch_data* foreign_watch_data =
            watch_data->foreign_watch_data;
        struct instrumentor_indirect1_f_ops_data* foreign_ops_data =
            instrumentor_indirect1_f_impl_get_ops_data(
                foreign_watch_data->ops_data->instrumentor_impl,
                (void*)*ops_p,
                ops_data);
        if(foreign_ops_data == NULL)
        {
            instrumentor_indirect1_wf_impl_unref_ops_data(
                instrumentor_impl,
                ops_data);
            return -ENOMEM;
        }
        
        instrumentor_indirect1_f_impl_unref_ops_data(
            foreign_watch_data->ops_data->instrumentor_impl,
            foreign_watch_data->ops_data);
        foreign_watch_data->ops_data = foreign_ops_data;
    }

    instrumentor_indirect1_wf_impl_unref_ops_data(
        instrumentor_impl, watch_data->ops_data);
    
    watch_data->ops_data = ops_data;
    return 0;
}


static void instrumentor_indirect1_wf_impl_clean_replacement(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    struct instrumentor_indirect1_wf_watch_data* watch_data,
    const void** ops_p)
{
    unsigned long flags;

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    kedr_coi_hash_table_remove_elem(
        &instrumentor_impl->hash_table_ops_p,
        &watch_data->hash_elem_ops_p);

    instrumentor_indirect1_wf_impl_unref_ops_data(instrumentor_impl,
        watch_data->ops_data);

    indirect_operations_fields_instrumented_remove(
        &watch_data->hash_elem_operations_field);

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
}

 
/*
 * Low level API of indirect instrumentor for implementation of
 * 'bind_object'.
 */

static int instrumentor_indirect1_replace_operations_from_foreign(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    struct instrumentor_indirect1_f_impl* foreign_instrumentor_impl,
    struct instrumentor_indirect1_wf_watch_data* watch_data_new,
    const void** ops_p,
    struct instrumentor_indirect1_f_watch_data* foreign_watch_data,
    size_t operation_offset,
    void **op_repl)
{
    unsigned long flags;
    int result;

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    result = instrumentor_indirect1_wf_impl_replace_operations_internal(
        instrumentor_impl,
        watch_data_new,
        ops_p,
        foreign_watch_data->ops_data->binded_ops_data);

    if(result)
    {
        *op_repl = instrumentor_indirect1_wf_impl_get_orig_operation_nodata_internal(
            instrumentor_impl,
            *ops_p,
            operation_offset);
        
        goto out;
    }

    *op_repl = instrumentor_indirect1_wf_impl_get_repl_operation_common(
        instrumentor_impl,
        watch_data_new->ops_data,
        operation_offset);

out:
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    return result;
}

static int instrumentor_indirect1_update_operations_from_foreign(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    struct instrumentor_indirect1_f_impl* foreign_instrumentor_impl,
    struct instrumentor_indirect1_wf_watch_data* watch_data,
    const void** ops_p,
    struct instrumentor_indirect1_f_watch_data* foreign_watch_data,
    size_t operation_offset,
    void **op_repl)
{
    unsigned long flags;
    int result;
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);

    result = instrumentor_indirect1_wf_impl_update_operations_internal(
        instrumentor_impl,
        watch_data,
        ops_p,
        foreign_watch_data->ops_data->binded_ops_data);

    if(result)
    {
        *op_repl = instrumentor_indirect1_wf_impl_get_orig_operation_nodata_internal(
            instrumentor_impl,
            *ops_p,
            operation_offset);
        
        goto out;
    }

    *op_repl = instrumentor_indirect1_wf_impl_get_repl_operation_common(
        instrumentor_impl,
        watch_data->ops_data,
        operation_offset);

out:
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    return result;
}

static int instrumentor_indirect1_restore_foreign_operations(
    struct instrumentor_indirect1_f_impl* foreign_instrumentor_impl,
    struct instrumentor_indirect1_f_watch_data* foreign_data,
    const void** ops_p,
    size_t operation_offset,
    void** op_orig)
{
    unsigned long flags;
    
    struct instrumentor_indirect1_wf_impl* binded_instrumentor_impl =
        foreign_instrumentor_impl->binded_instrumentor_impl;

    struct instrumentor_indirect1_wf_ops_data* binded_ops_data;

    spin_lock_irqsave(&binded_instrumentor_impl->ops_lock, flags);
    
    binded_ops_data = foreign_data->ops_data->binded_ops_data;

    if(binded_ops_data->ops == *ops_p)
    {
        *op_orig = operation_at_offset(binded_ops_data->ops_orig,
            operation_offset);
        goto out;
    }
        
    *op_orig = instrumentor_indirect1_wf_impl_get_orig_operation_nodata_internal(
        binded_instrumentor_impl,
        *ops_p,
        operation_offset);

out:
    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
    
    return IS_ERR(*op_orig) ? PTR_ERR(*op_orig) : 0;
}

static int instrumentor_indirect1_restore_foreign_operations_nodata(
    struct instrumentor_indirect1_f_impl* foreign_instrumentor_impl,
    const void** ops_p,
    size_t operation_offset,
    void** op_orig)
{
    unsigned long flags;
    
    struct instrumentor_indirect1_wf_impl* binded_instrumentor_impl =
        foreign_instrumentor_impl->binded_instrumentor_impl;

    struct instrumentor_indirect1_f_ops_data* foreign_ops_data;
    struct instrumentor_indirect1_wf_ops_data* ops_data;
    
    spin_lock_irqsave(&binded_instrumentor_impl->ops_lock, flags);
    
    ops_data = instrumentor_indirect1_wf_impl_find_ops_data(
        binded_instrumentor_impl,
        *ops_p);
    
    if(ops_data == NULL) goto no_repl;
    
    foreign_ops_data = instrumentor_indirect1_f_impl_find_ops_data(
        foreign_instrumentor_impl,
        ops_data);
            
    if(foreign_ops_data == NULL)
    {
        foreign_ops_data = list_prepare_entry(foreign_ops_data,
            &ops_data->foreign_ops_data, list);
    }
    list_for_each_entry_continue_reverse(foreign_ops_data,
        &ops_data->foreign_ops_data,
        list)
    {
        const struct kedr_coi_replacement* replacement;
        for(replacement = foreign_ops_data->instrumentor_impl->replacements;
            replacement->operation_offset != -1;
            replacement++)
        {
            if(replacement->operation_offset == operation_offset)
            {
                BUG_ON(replacement->mode != replace_all);
                *op_orig = replacement->repl;
                spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);
                return 0;
            }
        }
    }
    
no_repl:

    *op_orig = instrumentor_indirect1_wf_impl_get_orig_operation_nodata_internal(
            binded_instrumentor_impl,
            *ops_p,
            operation_offset);
    
    spin_unlock_irqrestore(&binded_instrumentor_impl->ops_lock, flags);    
    
    return IS_ERR(*op_orig) ? PTR_ERR(*op_orig) : 0;
}

static void* instrumentor_indirect1_chain_operation(
    struct instrumentor_indirect1_wf_impl* instrumentor_impl,
    struct instrumentor_indirect1_wf_watch_data* watch_data,
    const void** ops_p,
    size_t operation_offset)
{
    return instrumentor_indirect1_wf_impl_get_repl_operation_common(
        instrumentor_impl,
        watch_data->ops_data,
        operation_offset);
}

//** Interface of advanced indirect instrumentor for foreign support **//
static struct kedr_coi_foreign_instrumentor_watch_data*
instrumentor_indirect1_f_impl_iface_alloc_watch_data(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl)
{
    struct instrumentor_indirect1_f_watch_data* watch_data =
        kmalloc(sizeof(*watch_data), GFP_KERNEL);
    
    if(watch_data == NULL) return NULL;
    
    return &watch_data->base;
}
    
static void
instrumentor_indirect1_f_impl_iface_free_watch_data(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data)
{
    struct instrumentor_indirect1_f_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);

    kfree(watch_data_real);
}

static int
instrumentor_indirect1_f_impl_iface_replace_operations(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    struct instrumentor_indirect1_f_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_f_watch_data* watch_data_new_real =
        container_of(watch_data_new, typeof(*watch_data_new_real), base);
    
    if(*ops_p == NULL)
    {
        /* Special replacement of NULL-pointer as operations. */
        int result;
        *ops_p = instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl->binded_instrumentor_impl);
        result = instrumentor_indirect1_f_impl_replace_operations(
            instrumentor_impl,
            watch_data_new_real,
            ops_p);
        
        if(result)
        {
            /* Rollback */
            *ops_p = NULL;
            return result;
        }
        
        return 0;
    }
    else
    {
        return instrumentor_indirect1_f_impl_replace_operations(
            instrumentor_impl,
            watch_data_new_real,
            ops_p);
    }
}

static int
instrumentor_indirect1_f_impl_iface_update_operations(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect1_f_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_f_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    if(*ops_p == NULL)
    {
        /* Special replacement of NULL-pointer as operations. */
        int result;
        *ops_p = instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl->binded_instrumentor_impl);
        
        result = instrumentor_indirect1_f_impl_update_operations(
            instrumentor_impl,
            watch_data_real,
            ops_p);
        
        if(result)
        {
            /* Rollback */
            *ops_p = NULL;
            return result;
        }
        
        return 0;
    }
    return instrumentor_indirect1_f_impl_update_operations(
        instrumentor_impl,
        watch_data_real,
        ops_p);
}

static void
instrumentor_indirect1_f_impl_iface_clean_replacement(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect1_f_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_f_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    if(ops_p && (*ops_p == NULL))
    {
        *ops_p = instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl->binded_instrumentor_impl);
    }

    instrumentor_indirect1_f_impl_clean_replacement(
        instrumentor_impl,
        watch_data_real,
        ops_p);

    if(ops_p && (*ops_p == instrumentor_indirect1_wf_impl_get_null_ops(
        instrumentor_impl->binded_instrumentor_impl)))
    {
        *ops_p = NULL;
    }
}

static void instrumentor_indirect1_f_impl_iface_destroy_impl(
    struct kedr_coi_foreign_instrumentor_impl* iface_impl)
{
    struct instrumentor_indirect1_f_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);

    instrumentor_indirect1_f_impl_finalize(instrumentor_impl);
    
    kfree(instrumentor_impl);
}

static int
instrumentor_indirect1_f_impl_iface_restore_foreign_operations(
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_foreign_instrumentor_watch_data* foreign_data,
    const void** ops_p,
    size_t operation_offset,
    void** op_orig)
{
    int result;
    struct instrumentor_indirect1_f_impl* instrumentor_impl =
        container_of(iface_f_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_f_watch_data* foreign_data_real =
        container_of(foreign_data, typeof(*foreign_data_real), base);

    if(*ops_p == NULL)
        *ops_p = instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl->binded_instrumentor_impl);

    result = instrumentor_indirect1_restore_foreign_operations(
        instrumentor_impl,
        foreign_data_real,
        ops_p,
        operation_offset,
        op_orig);

    if(*ops_p == instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl->binded_instrumentor_impl))
    {
        *ops_p = NULL;
    }
    
    return result;
}

static int
instrumentor_indirect1_f_impl_iface_restore_foreign_operations_nodata(
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    const void** ops_p,
    size_t operation_offset,
    void** op_orig)
{
    int result;

    struct instrumentor_indirect1_f_impl* instrumentor_impl =
        container_of(iface_f_impl, typeof(*instrumentor_impl), base);
    
    if(*ops_p == NULL)
        *ops_p = instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl->binded_instrumentor_impl);

    result = instrumentor_indirect1_restore_foreign_operations_nodata(
        instrumentor_impl,
        ops_p,
        operation_offset,
        op_orig);

    if(*ops_p == instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl->binded_instrumentor_impl))
    {
        *ops_p = NULL;
    }

    return result;
}

static struct kedr_coi_foreign_instrumentor_impl_iface
instrumentor_indirect1_f_impl_iface =
{
    .alloc_watch_data = instrumentor_indirect1_f_impl_iface_alloc_watch_data,
    .free_watch_data = instrumentor_indirect1_f_impl_iface_free_watch_data,
    
    .replace_operations = instrumentor_indirect1_f_impl_iface_replace_operations,
    .update_operations = instrumentor_indirect1_f_impl_iface_update_operations,
    .clean_replacement = instrumentor_indirect1_f_impl_iface_clean_replacement,
    
    .destroy_impl = instrumentor_indirect1_f_impl_iface_destroy_impl,

    .restore_foreign_operations =
        instrumentor_indirect1_f_impl_iface_restore_foreign_operations,
    .restore_foreign_operations_nodata =
        instrumentor_indirect1_f_impl_iface_restore_foreign_operations_nodata
};

// Constructor for foreign instrumentor(for internal use)
static struct instrumentor_indirect1_f_impl*
instrumentor_indirect1_f_impl_create(
    struct instrumentor_indirect1_wf_impl* binded_instrumentor_impl,
    const struct kedr_coi_replacement* replacements)
{
    int result;
    struct instrumentor_indirect1_f_impl* instrumentor_impl =
        kmalloc(sizeof(*instrumentor_impl), GFP_KERNEL);
    
    if(instrumentor_impl == NULL)
    {
        pr_err("Failed to allocate structure for foreign instrumentor.");
        return NULL;
    }
    
    result = instrumentor_indirect1_f_impl_init(instrumentor_impl,
        binded_instrumentor_impl,
        replacements);
    
    if(result)
    {
        kfree(instrumentor_impl);
        return NULL;
    }
    
    instrumentor_impl->base.interface = &instrumentor_indirect1_f_impl_iface;
    
    return instrumentor_impl;
}

static struct kedr_coi_instrumentor_watch_data*
instrumentor_indirect1_wf_impl_iface_alloc_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    struct instrumentor_indirect1_wf_watch_data* watch_data =
        kmalloc(sizeof(*watch_data), GFP_KERNEL);
    
    if(watch_data == NULL) return NULL;
    
    return &watch_data->base;
}
    
static void
instrumentor_indirect1_wf_impl_iface_free_watch_data(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    struct instrumentor_indirect1_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    kfree(watch_data_real);
}

static int
instrumentor_indirect1_wf_impl_iface_replace_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data_new,
    const void** ops_p)
{
    int result;
    unsigned long flags;
    
    struct instrumentor_indirect1_wf_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_wf_watch_data* watch_data_new_real =
        container_of(watch_data_new, typeof(*watch_data_new_real), base);
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    if(*ops_p == NULL)
    {
        *ops_p = instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl);
    }

    result = instrumentor_indirect1_wf_impl_replace_operations_internal(
        instrumentor_impl,
        watch_data_new_real,
        ops_p,
        NULL);
    
    if(result)
    {
        /* Rolback if needed */
        if(*ops_p == instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl))
        {
            *ops_p = NULL;
        }
    }

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    return result;
}

static int
instrumentor_indirect1_wf_impl_iface_update_operations(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    int result;
    unsigned long flags;
    
    struct instrumentor_indirect1_wf_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);

    if(*ops_p == NULL)
    {
        *ops_p = instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl);
    }
    
    result = instrumentor_indirect1_wf_impl_update_operations_internal(
        instrumentor_impl,
        watch_data_real,
        ops_p,
        NULL);
    
    if(result)
    {
        /* Rolback if needed */
        if(*ops_p == instrumentor_indirect1_wf_impl_get_null_ops(
            instrumentor_impl))
        {
            *ops_p = NULL;
        }
    }
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    return result;
}

static void
instrumentor_indirect1_wf_impl_iface_clean_replacement(
    struct kedr_coi_instrumentor_impl* iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p)
{
    struct instrumentor_indirect1_wf_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);
    
    instrumentor_indirect1_wf_impl_clean_replacement(
        instrumentor_impl,
        watch_data_real,
        ops_p);

    if(ops_p && (*ops_p == instrumentor_indirect1_wf_impl_get_null_ops(
        instrumentor_impl)))
    {
        *ops_p = NULL;
    }
}


static void instrumentor_indirect1_wf_impl_iface_destroy_impl(
    struct kedr_coi_instrumentor_impl* iface_impl)
{
    struct instrumentor_indirect1_wf_impl* instrumentor_impl =
        container_of(iface_impl, typeof(*instrumentor_impl), base);

    instrumentor_indirect1_wf_impl_finalize(instrumentor_impl);
    
    kfree(instrumentor_impl);
}

static void* instrumentor_indirect1_wf_impl_iface_get_orig_operation(
    struct kedr_coi_instrumentor_impl* in_iface_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void* ops,
    size_t operation_offset)
{
    struct instrumentor_indirect1_wf_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);

    return operation_at_offset(watch_data_real->ops_data->ops_orig,
        operation_offset);
}

static void* instrumentor_indirect1_wf_impl_iface_get_orig_operation_nodata(
    struct kedr_coi_instrumentor_impl* in_iface_impl,
    const void* ops,
    size_t operation_offset)
{
    void* op_orig;
    unsigned long flags;
    
    struct instrumentor_indirect1_wf_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    if(ops == NULL)
        ops = instrumentor_indirect1_wf_impl_get_null_ops(instrumentor_impl);

    op_orig = instrumentor_indirect1_wf_impl_get_orig_operation_nodata_internal(
        instrumentor_impl,
        ops,
        operation_offset);
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return op_orig;
}


static struct kedr_coi_foreign_instrumentor_impl*
instrumentor_indirect1_wf_impl_iface_foreign_instrumentor_impl_create(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    const struct kedr_coi_replacement* replacements)
{
    struct instrumentor_indirect1_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_f_impl* foreign_instrumentor_impl =
        instrumentor_indirect1_f_impl_create(
            instrumentor_impl,
            replacements);
    
    if(foreign_instrumentor_impl == NULL) return NULL;
    
    return &foreign_instrumentor_impl->base;
}

static int
instrumentor_indirect1_wf_impl_iface_replace_operations_from_foreign(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data_new,
    const void** ops_p,
    struct kedr_coi_foreign_instrumentor_watch_data* foreign_data,
    size_t operation_offset,
    void** op_repl)
{
    int result;
    
    struct instrumentor_indirect1_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_f_impl* foreign_instrumentor_impl =
        container_of(iface_f_impl, typeof(*foreign_instrumentor_impl), base);
    
    struct instrumentor_indirect1_wf_watch_data* watch_data_new_real =
        container_of(watch_data_new, typeof(*watch_data_new_real), base);

    struct instrumentor_indirect1_f_watch_data* foreign_data_real =
        container_of(foreign_data, typeof(*foreign_data_real), base);

    if(*ops_p == NULL)
        *ops_p = instrumentor_indirect1_wf_impl_get_null_ops(instrumentor_impl);
    
    result = instrumentor_indirect1_replace_operations_from_foreign(
        instrumentor_impl,
        foreign_instrumentor_impl,
        watch_data_new_real,
        ops_p,
        foreign_data_real,
        operation_offset,
        op_repl);
    
    if(result)
    {
        if(*ops_p == instrumentor_indirect1_wf_impl_get_null_ops(instrumentor_impl))
            *ops_p = NULL;
    }

    return result;
}

static int
instrumentor_indirect1_wf_impl_iface_update_operations_from_foreign(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p,
    struct kedr_coi_foreign_instrumentor_watch_data* foreign_data,
    size_t operation_offset,
    void** op_repl)
{
    int result;
    
    struct instrumentor_indirect1_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_f_impl* foreign_instrumentor_impl =
        container_of(iface_f_impl, typeof(*foreign_instrumentor_impl), base);
    
    struct instrumentor_indirect1_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);

    struct instrumentor_indirect1_f_watch_data* foreign_data_real =
        container_of(foreign_data, typeof(*foreign_data_real), base);

    if(*ops_p == NULL)
        *ops_p = instrumentor_indirect1_wf_impl_get_null_ops(instrumentor_impl);

    result = instrumentor_indirect1_update_operations_from_foreign(
        instrumentor_impl,
        foreign_instrumentor_impl,
        watch_data_real,
        ops_p,
        foreign_data_real,
        operation_offset,
        op_repl);

    if(result)
    {
        if(*ops_p == instrumentor_indirect1_wf_impl_get_null_ops(instrumentor_impl))
            *ops_p = NULL;
    }

    return result;
}

static int
instrumentor_indirect1_wf_impl_iface_chain_operation(
    struct kedr_coi_instrumentor_impl* iface_wf_impl,
    struct kedr_coi_foreign_instrumentor_impl* iface_f_impl,
    struct kedr_coi_instrumentor_watch_data* watch_data,
    const void** ops_p,
    size_t operation_offset,
    void** op_chained)
{
    struct instrumentor_indirect1_wf_impl* instrumentor_impl =
        container_of(iface_wf_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_wf_watch_data* watch_data_real =
        container_of(watch_data, typeof(*watch_data_real), base);

    *op_chained = instrumentor_indirect1_chain_operation(
        instrumentor_impl,
        watch_data_real,
        ops_p,
        operation_offset);
    
    return IS_ERR(*op_chained)? PTR_ERR(*op_chained) : 0;
}

static struct kedr_coi_instrumentor_with_foreign_impl_iface
instrumentor_indirect1_wf_impl_iface =
{
    .base =
    {
        .alloc_watch_data = instrumentor_indirect1_wf_impl_iface_alloc_watch_data,
        .free_watch_data = instrumentor_indirect1_wf_impl_iface_free_watch_data,
        
        .replace_operations = instrumentor_indirect1_wf_impl_iface_replace_operations,
        .update_operations = instrumentor_indirect1_wf_impl_iface_update_operations,
        .clean_replacement = instrumentor_indirect1_wf_impl_iface_clean_replacement,
        
        .destroy_impl = instrumentor_indirect1_wf_impl_iface_destroy_impl,

        .get_orig_operation = instrumentor_indirect1_wf_impl_iface_get_orig_operation,
        .get_orig_operation_nodata =
            instrumentor_indirect1_wf_impl_iface_get_orig_operation_nodata,
    },
    .foreign_instrumentor_impl_create = instrumentor_indirect1_wf_impl_iface_foreign_instrumentor_impl_create,
    .replace_operations_from_foreign =
        instrumentor_indirect1_wf_impl_iface_replace_operations_from_foreign,
    .update_operations_from_foreign =
        instrumentor_indirect1_wf_impl_iface_update_operations_from_foreign,
    .chain_operation =
        instrumentor_indirect1_wf_impl_iface_chain_operation,
};


static struct kedr_coi_instrumentor_with_foreign*
instrumentor_indirect1_wf_create(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result;
    struct kedr_coi_instrumentor_with_foreign* instrumentor;
    struct instrumentor_indirect1_wf_impl* instrumentor_impl;
    
    if(replacements == NULL || replacements[0].operation_offset == -1)
    {
        return instrumentor_with_foreign_empty_create();
    }
    
    instrumentor_impl = kmalloc(sizeof(*instrumentor_impl), GFP_KERNEL);
    
    if(instrumentor_impl == NULL)
    {
        pr_err("Failed to allocate structure for instrumentor with foreign support.");
        return NULL;
    }
    
    result = instrumentor_indirect1_wf_impl_init(
        instrumentor_impl,
        operations_struct_size,
        replacements);
    
    if(result)
    {
        kfree(instrumentor_impl);
        return NULL;
    }
    
    instrumentor_impl->base.interface =
        &instrumentor_indirect1_wf_impl_iface.base;
    
    instrumentor = kedr_coi_instrumentor_with_foreign_create(
        &instrumentor_impl->base);
    
    if(instrumentor == NULL)
    {
        instrumentor_indirect1_wf_impl_finalize(instrumentor_impl);
        kfree(instrumentor_impl);
        return NULL;
    }
    
    return instrumentor;
}
//****************Implementation of global maps**********************//

//**Implementation of the global map for direct objects***//
static struct kedr_coi_hash_table operations_directly_instrumented;

DEFINE_SPINLOCK(operations_directly_instrumented_lock);

int operations_directly_instrumented_add_unique(
    struct kedr_coi_hash_elem* object_elem)
{
    unsigned long flags;
    
    spin_lock_irqsave(&operations_directly_instrumented_lock, flags);
    if(kedr_coi_hash_table_find_elem(&operations_directly_instrumented,
        object_elem->key))
    {
        spin_unlock_irqrestore(&operations_directly_instrumented_lock, flags);
        return -EBUSY;
    }
    kedr_coi_hash_table_add_elem(&operations_directly_instrumented,
        object_elem);
    
    spin_unlock_irqrestore(&operations_directly_instrumented_lock, flags);

    return 0;
}

void operations_directly_instrumented_remove(
    struct kedr_coi_hash_elem* object_elem)
{
    unsigned long flags;
    
    spin_lock_irqsave(&operations_directly_instrumented_lock, flags);
    kedr_coi_hash_table_remove_elem(&operations_directly_instrumented,
        object_elem);
    spin_unlock_irqrestore(&operations_directly_instrumented_lock, flags);
}


//**Implementation of the global map for indirect operations fields***//
static struct kedr_coi_hash_table indirect_operations_fields_instrumented;
DEFINE_SPINLOCK(indirect_operations_fields_instrumented_lock);

int indirect_operations_fields_instrumented_add_unique(
    struct kedr_coi_hash_elem* hash_elem_operations_field)
{
    unsigned long flags;

    //pr_info("Add operations field %p to the table %p.",
    //    hash_elem_operations_field->key, &indirect_operations_fields_instrumented);
    
    spin_lock_irqsave(&indirect_operations_fields_instrumented_lock, flags);
    if(kedr_coi_hash_table_find_elem(&indirect_operations_fields_instrumented,
        hash_elem_operations_field->key))
    {
        spin_unlock_irqrestore(&indirect_operations_fields_instrumented_lock, flags);
        return -EBUSY;
    }
    
    kedr_coi_hash_table_add_elem(&indirect_operations_fields_instrumented,
        hash_elem_operations_field);
    spin_unlock_irqrestore(&indirect_operations_fields_instrumented_lock, flags);
    return 0;
}

void indirect_operations_fields_instrumented_remove(
    struct kedr_coi_hash_elem* hash_elem_operations_field)
{
    unsigned long flags;
    
    //pr_info("Remove operations field %p from the table %p.",
    //    hash_elem_operations_field->key, &indirect_operations_fields_instrumented);

    spin_lock_irqsave(&indirect_operations_fields_instrumented_lock, flags);
    kedr_coi_hash_table_remove_elem(&indirect_operations_fields_instrumented,
        hash_elem_operations_field);
    spin_unlock_irqrestore(&indirect_operations_fields_instrumented_lock, flags);
}

void indirect_operations_fields_instrumented_transmit(
    struct kedr_coi_hash_elem* hash_elem_operations_field,
    struct kedr_coi_hash_elem* hash_elem_operations_field_new)
{
    unsigned long flags;
    
    spin_lock_irqsave(&indirect_operations_fields_instrumented_lock, flags);
    kedr_coi_hash_table_transmit_elem(&indirect_operations_fields_instrumented,
        hash_elem_operations_field, hash_elem_operations_field_new);
    spin_unlock_irqrestore(&indirect_operations_fields_instrumented_lock, flags);
}


//**Implementation of the global map for indirect operations***//
static struct kedr_coi_hash_table indirect_operations_instrumented;
DEFINE_SPINLOCK(indirect_operations_instrumented_lock);

int indirect_operations_instrumented_add_unique(
    struct kedr_coi_hash_elem* hash_elem_operations)
{
    unsigned long flags;

    spin_lock_irqsave(&indirect_operations_instrumented_lock, flags);
    if(kedr_coi_hash_table_find_elem(&indirect_operations_instrumented,
        hash_elem_operations->key))
    {
        spin_unlock_irqrestore(&indirect_operations_instrumented_lock, flags);
        return -EBUSY;
    }
    
    kedr_coi_hash_table_add_elem(&indirect_operations_instrumented,
        hash_elem_operations);
    spin_unlock_irqrestore(&indirect_operations_instrumented_lock, flags);
    return 0;
}

void indirect_operations_instrumented_remove(
    struct kedr_coi_hash_elem* hash_elem_operations)
{
    unsigned long flags;
    
    spin_lock_irqsave(&indirect_operations_instrumented_lock, flags);
    kedr_coi_hash_table_remove_elem(&indirect_operations_instrumented,
        hash_elem_operations);
    spin_unlock_irqrestore(&indirect_operations_instrumented_lock, flags);
}

//*******Initialize and destroy instrumentors support*****************//
int kedr_coi_instrumentors_init(void)
{
    int result;
    
    result = kedr_coi_hash_table_init(&indirect_operations_fields_instrumented);
    if(result) goto err_indirect_fields;
    
    result = kedr_coi_hash_table_init(&indirect_operations_instrumented);
    if(result) goto err_indirect;
    
    result = kedr_coi_hash_table_init(&operations_directly_instrumented);
    if(result) goto err_direct;
    
    return 0;

err_direct:
    kedr_coi_hash_table_destroy(&indirect_operations_instrumented, NULL);
err_indirect:
    kedr_coi_hash_table_destroy(&indirect_operations_fields_instrumented, NULL);
err_indirect_fields:
    return result;
}

void kedr_coi_instrumentors_destroy(void)
{
    kedr_coi_hash_table_destroy(&operations_directly_instrumented, NULL);
    kedr_coi_hash_table_destroy(&indirect_operations_instrumented, NULL);
    kedr_coi_hash_table_destroy(&indirect_operations_fields_instrumented, NULL);
}

//*********** API for create instrumentors ************************//
struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_direct(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return instrumentor_direct_create(operations_struct_size, replacements);
}

struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_indirect(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return instrumentor_indirect_create(
        operations_struct_size,
        replacements);
}

struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_indirect1(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return instrumentor_indirect1_create(
        operations_struct_size,
        replacements);
}

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return instrumentor_indirect_wf_create(
        operations_struct_size,
        replacements);
}

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect1(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return instrumentor_indirect1_wf_create(
        operations_struct_size,
        replacements);
}