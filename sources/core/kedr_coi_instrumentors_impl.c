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

// Get operation at given offset(in the operations structure or in the object)
static inline void*
operation_at_offset(const void* ops, size_t operation_offset)
{
    return *((void**)((const char*)ops + operation_offset));
}

static inline void**
operation_at_offset_p(void* ops, size_t operation_offset)
{
    return ((void**)((char*)ops + operation_offset));
}

/**********************************************************************/
/***********Instrumentor which does not instrument at all**************/
/**********************************************************************/

/*
 * Such instrumentor is a fallback for instrumentor's constructors
 * when nothing to replace.
 */

// Interface of emtpy instrumentor as common one
static struct kedr_coi_instrumentor_object_data*
instrumentor_empty_impl_i_iface_alloc_object_data(
    struct kedr_coi_object* i_iface_impl)
{
    return kmalloc(sizeof(struct kedr_coi_instrumentor_object_data), GFP_KERNEL);
}

static void instrumentor_empty_impl_i_iface_free_object_data(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    kfree(object_data);
}

static int instrumentor_empty_impl_i_iface_replace_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object)
{
    return 0;// nothing to replace
}

static void instrumentor_empty_impl_i_iface_clean_replacement(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object,
    int norestore)
{
}


static int instrumentor_empty_impl_i_iface_update_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object)
{
    return 0;// always up-to-date
}

static void instrumentor_empty_impl_i_iface_destroy_impl(
    struct kedr_coi_object* i_iface_impl)
{
    kfree(i_iface_impl);
}

//*********** Creation of the empty instrumentor as normal**********************

struct kedr_coi_instrumentor_normal_impl_iface instrumentor_empty_in_iface =
{
    .i_iface =
    {
        .alloc_object_data = instrumentor_empty_impl_i_iface_alloc_object_data,
        .free_object_data = instrumentor_empty_impl_i_iface_free_object_data,
        
        .replace_operations = instrumentor_empty_impl_i_iface_replace_operations,
        .clean_replacement = instrumentor_empty_impl_i_iface_clean_replacement,
        .update_operations = instrumentor_empty_impl_i_iface_update_operations,
        .destroy_impl = instrumentor_empty_impl_i_iface_destroy_impl
    }
    // Other fields should not be accessed
};

// Constructor for empty instrumentor(for internal use)
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


//*********** Empty instrumentor with foreign support*****************//
static struct kedr_coi_foreign_instrumentor_impl_iface
instrumentor_empty_impl_if_iface =
{
    .i_iface =
    {
        .alloc_object_data = instrumentor_empty_impl_i_iface_alloc_object_data,
        .free_object_data = instrumentor_empty_impl_i_iface_free_object_data,
        
        .replace_operations = instrumentor_empty_impl_i_iface_replace_operations,
        .clean_replacement = instrumentor_empty_impl_i_iface_clean_replacement,
        .update_operations = instrumentor_empty_impl_i_iface_update_operations,
        
        .destroy_impl = instrumentor_empty_impl_i_iface_destroy_impl
    },
    // Other fields should not be accesssed
};

static struct kedr_coi_object*
instrumentor_empty_impl_iwf_iface_create_foreign_indirect(
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
    
    instrumentor_empty_impl->interface =
        &instrumentor_empty_impl_if_iface.i_iface.base;
    
    return instrumentor_empty_impl;
}


static struct kedr_coi_instrumentor_with_foreign_impl_iface
instrumentor_empty_impl_iwf_iface =
{
    .in_iface =
    {
        .i_iface =
        {
            .alloc_object_data = instrumentor_empty_impl_i_iface_alloc_object_data,
            .free_object_data = instrumentor_empty_impl_i_iface_free_object_data,
            
            .replace_operations = instrumentor_empty_impl_i_iface_replace_operations,
            .clean_replacement = instrumentor_empty_impl_i_iface_clean_replacement,
            .update_operations = instrumentor_empty_impl_i_iface_update_operations,
            
            .destroy_impl = instrumentor_empty_impl_i_iface_destroy_impl
        },
        // Other fields should not be accesssed
    },
    .create_foreign_indirect = instrumentor_empty_impl_iwf_iface_create_foreign_indirect,
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
    
    instrumentor_empty_impl->interface =
        &instrumentor_empty_impl_iwf_iface.in_iface.i_iface.base;

    instrumentor = kedr_coi_instrumentor_with_foreign_create(instrumentor_empty_impl);
    if(instrumentor == NULL)
    {
        kfree(instrumentor_empty_impl);
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
 * In this concrete case, map contains pointers to objects.
 */

/*
 * Add object(as key) to the hash table of objects instrumented.
 * 
 * If object already exists in the table, return -EBUSY.
 */
static int objects_directly_instrumented_add_unique(
    struct kedr_coi_hash_elem* object_elem);

/*
 * Remove object(as key) from the hash table of objects instrumented*/
static void objects_directly_instrumented_remove(
    struct kedr_coi_hash_elem* object_elem);

// Per-object data of direct instrumentor implementation.
struct instrumentor_direct_object_data
{
    struct kedr_coi_instrumentor_object_data base;

    struct kedr_coi_hash_elem object_elem;
    // Object with filled original operations(other fields are uninitialized).
    void* object_ops_orig;
};

// Object type of direct instrumentor implementation.
struct instrumentor_direct_impl
{
    struct kedr_coi_object base;
    size_t object_struct_size;
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

// Prepare newly allocated instance of per-object data for work with.
static int
instrumentor_direct_impl_init_object_data(
    struct instrumentor_direct_impl* instrumentor,
    struct instrumentor_direct_object_data* object_data)
{
    object_data->object_ops_orig = kmalloc(
        instrumentor->object_struct_size, GFP_KERNEL);
    if(object_data->object_ops_orig == NULL)
    {
        return -ENOMEM;
    }
    
    return 0;
}

/*
 *  Destroy data in the instance of per-object data.
 * (Instance itself isn't freed.)
 */
static void
instrumentor_direct_impl_destroy_object_data(
    struct instrumentor_direct_impl* instrumentor,
    struct instrumentor_direct_object_data* object_data)
{
    kfree(object_data->object_ops_orig);
}

/*
 * Initialize newly allocated instance of instrumentor implementation.
 */
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


/*
 * Destroy instance of instrumentor implementation.
 * (Instance itself isn't freed.)
 */
static void
instrumentor_direct_impl_finalize(
    struct instrumentor_direct_impl* instrumentor)
{
}


/*
 * Replace operations in the object, filling data for it.
 */
static int
instrumentor_direct_impl_replace_operations(
    struct instrumentor_direct_impl* instrumentor_impl,
    struct instrumentor_direct_object_data* object_data_new,
    void* object)
{
    const struct kedr_coi_replacement* replacement;
    char* object_ops_orig;
    
    int result;
    
    // Check collisions with other instrumentors.
    kedr_coi_hash_elem_init(&object_data_new->object_elem, object);
    
    result = objects_directly_instrumented_add_unique(
        &object_data_new->object_elem);

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

    // Replace operations in the objects and store its old values.
    object_ops_orig = object_data_new->object_ops_orig;

    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        size_t operation_offset = replacement->operation_offset;
        void** operation_p = operation_at_offset_p(
            object, operation_offset);
         
        *operation_at_offset_p(object_ops_orig, operation_offset) =
            *operation_p;
        
        *operation_p = replacement->repl;
    }
    
    return 0;
}

/*
 *  Clean operations replacement in the object and restore operation
 * if needed.
 */
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
            void** operation_p = operation_at_offset_p(
                object, operation_offset);
             
            if(*operation_p == replacement->repl)
                *operation_p = operation_at_offset(
                    object_ops_orig, operation_offset);
        }
    }
    
    objects_directly_instrumented_remove(&object_data->object_elem);
}


/*
 * Update replaced operations in the object.
 */
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
        void** operation_p = operation_at_offset_p(
            object, operation_offset);
         
        if(*operation_p != replacement->repl)
        {
            *operation_at_offset_p(
                object_ops_orig, operation_offset) = *operation_p;
            
            *operation_p = replacement->repl;
        }
    }
    
    return 0;
}


static void*
instrumentor_direct_impl_get_orig_operation(
    struct instrumentor_direct_impl* instrumentor_impl,
    struct instrumentor_direct_object_data* object_data,
    size_t operation_offset)
{
    return operation_at_offset(object_data->object_ops_orig,
        operation_offset);
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

//********* Interface of direct instrumentor as normal******************
static void*
instrumentor_direct_impl_in_iface_get_orig_operation(
    struct kedr_coi_object* in_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    const void* object,
    size_t operation_offset)
{
    struct instrumentor_direct_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_direct_impl_get_orig_operation(
        instrumentor_impl,
        object_data_real,
        operation_offset);
}

static void*
instrumentor_direct_impl_in_iface_get_orig_operation_nodata(
    struct kedr_coi_object* in_iface_impl,
    const void* object,
    size_t operation_offset)
{
    //Shouldn't be called
    BUG();
    
    return ERR_PTR(-EINVAL);
}


static void instrumentor_direct_impl_in_iface_destroy_impl(
    struct kedr_coi_object* i_iface_impl)
{
    struct instrumentor_direct_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    instrumentor_direct_impl_finalize(instrumentor_impl);
    
    kfree(instrumentor_impl);
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


//Constructor for direct instrumentor
static struct kedr_coi_instrumentor_normal*
instrumentor_direct_create(
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
        instrumentor_direct_impl_finalize(instrumentor_impl);
        kfree(instrumentor_impl);
        return NULL;
    }

    return instrumentor;
}


/**********************************************************************/
/*********Instrumentor for indirect operations (mostly used)***********/
/**********************************************************************/

// Return pointer to the operations struct in the object
static const void* indirect_operations(const void* object,
    size_t operations_field_offset)
{
    return *(const void**)((const char*) object + operations_field_offset);
}

static const void** indirect_operations_p(void* object,
    size_t operations_field_offset)
{
    return (const void**)((char*) object + operations_field_offset);
}

/*
 * Global map for prevent different indirect instrumentors to perform
 * operations replacements which has undesireable effect on each other.
 * 
 * In this concrete case, map contains pointers to operations fields.
 */

/*
 * Add pointer to the operation field(as key) to the map of
 * instrumented operations fields.
 * 
 * If field already in the map, return -EBUSY.
 */
static int indirect_operations_fields_instrumented_add_unique(
    struct kedr_coi_hash_elem* operations_field_elem);

static void indirect_operations_fields_instrumented_remove(
    struct kedr_coi_hash_elem* operations_field_elem);


/*
 * Per-operations data for instrumentor.
 * 
 * These data contain all information about replaced operations,
 * 
 * For search, these data are organized into to hash tables:
 * 1) table with replaced operations(mainly used)
 * 2) table with original operations(useful in some rare cases).
 * 
 * Per-object data share per-operations data for same operations struct.
 * 
 * Note that after initializing content of the instance is constant(final)
 * except 'refs' field and fields with hash table elements.
 */
struct instrumentor_indirect_ops_data
{
    const void* ops_orig;
    void* ops_repl;
    //key - replaced operations
    struct kedr_coi_hash_elem hash_elem_ops_repl;
    //key - original operations
    struct kedr_coi_hash_elem hash_elem_ops_orig;
    // refcount for share these data
    int refs;
};


/*
 *  Per-object data for instrumentor.
 * 
 * Contain reference to per-operations data plus key for global map
 * of instrumented operations fields.
 */

struct instrumentor_indirect_object_data
{
    struct kedr_coi_instrumentor_object_data base;

    struct kedr_coi_hash_elem operations_field_elem;
    // Pointer to the operations data, which is referenced by this object
    struct instrumentor_indirect_ops_data* ops_data;
};

/*
 * Implementation of the indirect instrumentor.
 */
struct instrumentor_indirect_impl
{
    struct kedr_coi_object base;
    // hash table with replaced operations as keys
    struct kedr_coi_hash_table hash_table_ops_repl;
    // hash table with original operations as keys
    struct kedr_coi_hash_table hash_table_ops_orig;
    /*
     * Protect tables with operations data from concurrent access.
     * Also protect 'refs' field of per-operations data from concurrent
     * access.
     */
    spinlock_t ops_lock;
    
    size_t operations_field_offset;
    size_t operations_struct_size;
    /*
     *  NOTE: 'replacements' field is not NULL
     * (without replacements empty variant of instrumentor is created).
     */
    const struct kedr_coi_replacement* replacements;
};

// Initialize instance of indirect instrumentor
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

/*
 * Destroy instance of indirect instrumentor.
 * (Instance itself isn't freed).
 */
static void
instrumentor_indirect_impl_finalize(
    struct instrumentor_indirect_impl* instrumentor_impl)
{
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_repl, NULL);
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops_orig, NULL);
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
    void* ops_repl;
    const struct kedr_coi_replacement* replacement;
    
    struct instrumentor_indirect_ops_data* ops_data =
        kmalloc(sizeof(*ops_data), GFP_ATOMIC);
    
    if(ops_data == NULL) return NULL;
    
    ops_repl = kmalloc(instrumentor_impl->operations_struct_size, GFP_ATOMIC);
    
    if(ops_repl == NULL)
    {
        kfree(ops_data);
        return NULL;
    }
    
    ops_data->ops_repl = ops_repl;
    ops_data->ops_orig = ops_orig;
    
    ops_data->refs = 1;
    

    kedr_coi_hash_elem_init(&ops_data->hash_elem_ops_orig, ops_orig);

    kedr_coi_hash_table_add_elem(
        &instrumentor_impl->hash_table_ops_orig,
        &ops_data->hash_elem_ops_orig);

    kedr_coi_hash_elem_init(&ops_data->hash_elem_ops_repl, ops_repl);
    
    kedr_coi_hash_table_add_elem(
        &instrumentor_impl->hash_table_ops_repl,
        &ops_data->hash_elem_ops_repl);


    memcpy(ops_repl, ops_orig,
        instrumentor_impl->operations_struct_size);
    
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        *operation_at_offset_p(ops_repl, replacement->operation_offset) =
            replacement->repl;
    }

    return ops_data;
}

/*
 *  Destroy instance of per-operations data and free its structure.
 * 
 * Should be executed under lock.
 */
static void
instrumentor_indirect_impl_remove_ops_data(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_ops_data* ops_data)
{
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
    
    instrumentor_indirect_impl_remove_ops_data(
        instrumentor_impl, ops_data);
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
        instrumentor_indirect_impl_ref_ops_data(
            instrumentor_impl, ops_data);
        return ops_data;
    }

    ops_data = instrumentor_indirect_impl_add_ops_data(
        instrumentor_impl, object_ops);

    if(ops_data == NULL) return NULL;
    
    return ops_data;
}

/*
 * This helper is useful for the cases when we doesn't have information
 * about some operations structure.
 * 
 * Helper check whether operation at the given offset is not our
 * replacement operation. It it is so, it return that operation.
 * Otherwise ERR_PTR() is returned.
 * 
 * Should be executed under lock(for make sure that object operations
 * will not changed during testing of them).
 */
static void* instrumentor_indirect_impl_get_operation_not_replaced(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* object_ops,
    size_t operation_offset)
{
    const struct kedr_coi_replacement* replacement;
    
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        if(replacement->operation_offset == operation_offset)
        {
            void* op = operation_at_offset(object_ops, operation_offset);
            if(replacement->repl != op)
            {
                return op;
            }
            // Cannot determine original operation
            return ERR_PTR(-ENOENT);
        }
    }
    // operation_offset is invalid
    return ERR_PTR(-EINVAL);
}

//***************** Low level API of indirect instrumentor***********//

static int
instrumentor_indirect_impl_replace_operations(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_object_data* object_data_new,
    void* object)
{
    unsigned long flags;
    struct instrumentor_indirect_ops_data* ops_data;
    
    int result;
    const void** object_ops_p = indirect_operations_p(
        object,
        instrumentor_impl->operations_field_offset);

    /* 
     * Check for collisions with another instrumentors.
     */

    kedr_coi_hash_elem_init(
        &object_data_new->operations_field_elem,
        object_ops_p);

    result = indirect_operations_fields_instrumented_add_unique(
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

        indirect_operations_fields_instrumented_remove(
            &object_data_new->operations_field_elem);
        return -ENOMEM;
    }

    object_data_new->ops_data = ops_data;

    *object_ops_p = ops_data->ops_repl;

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return 0;
}

/*
 *  Restore given operations according to per-operations data.
 * 
 * Do not change reference count of these data.
 * 
 * Should be executed under lock or with refs on per-operations
 * data instance.
 */
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
        const void** object_ops_p = indirect_operations_p(
            object,
            instrumentor_impl->operations_field_offset);
        
        /* 
         * 'ops_data' is reffered by current object, access to which is
         * serialized by the caller.
         * 
         * So, lock doesn't need.
         */
        instrumentor_indirect_impl_restore_ops(
            object_data->ops_data,
            object_ops_p);
    }

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    instrumentor_indirect_impl_unref_ops_data(instrumentor_impl,
        object_data->ops_data);
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    indirect_operations_fields_instrumented_remove(
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
    
    object_ops_p = indirect_operations_p(object,
        instrumentor_impl->operations_field_offset);
    
    ops_data = object_data->ops_data;
    
    /* 
     * 'ops_data' is reffered by current object, access to which is
     * serialized by the caller.
     * 
     * So, lock doesn't need for access to operations data's
     * final fields.
     */
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

//******Low-level API of the indirect instrumentor as normal***********//
static void* instrumentor_indirect_impl_get_orig_operation_nodata(
    struct instrumentor_indirect_impl* instrumentor_impl,
    const void* object,
    size_t operation_offset)
{
    void* op_orig;
    unsigned long flags;
    struct instrumentor_indirect_ops_data* ops_data;
    const void* object_ops;
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    object_ops = indirect_operations(object,
        instrumentor_impl->operations_field_offset);
    
    ops_data = instrumentor_indirect_impl_find_ops_data(
        instrumentor_impl,
        object_ops);

    if(ops_data != NULL)
    {
        op_orig = operation_at_offset(ops_data->ops_orig, operation_offset);
    }
    else
    {
        op_orig = instrumentor_indirect_impl_get_operation_not_replaced(
            instrumentor_impl,
            object_ops,
            operation_offset);
    }
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return op_orig;
}

//********Interface of the indirect instrumentor as normal*************
static void* instrumentor_indirect_impl_in_iface_get_orig_operation(
    struct kedr_coi_object* in_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    const void* object,
    size_t operation_offset)
{
    struct instrumentor_indirect_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    return operation_at_offset(
        object_data_real->ops_data->ops_orig,
        operation_offset);
}

static void* instrumentor_indirect_impl_in_iface_get_orig_operation_nodata(
    struct kedr_coi_object* in_iface_impl,
    const void* object,
    size_t operation_offset)
{
    struct instrumentor_indirect_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_indirect_impl_get_orig_operation_nodata(
        instrumentor_impl,
        object,
        operation_offset);
}

static void instrumentor_indirect_impl_in_iface_destroy_impl(
    struct kedr_coi_object* i_iface_impl)
{
    struct instrumentor_indirect_impl* impl_real =
        container_of(i_iface_impl, typeof(*impl_real), base);
    
    instrumentor_indirect_impl_finalize(impl_real);
    
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
static struct kedr_coi_instrumentor_normal*
instrumentor_indirect_create(
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
        instrumentor_indirect_impl_finalize(instrumentor_impl);
        kfree(instrumentor_impl);
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
    // List organization of foreign instrumentors
    struct list_head list;
};

struct instrumentor_indirect_with_foreign_impl
{
    struct instrumentor_indirect_impl base;
    // List of foreign instrumentors binded with this
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

static void foreign_instrumentor_indirect_impl_finalize(
    struct foreign_instrumentor_indirect_impl* instrumentor_impl)
{
    if(!list_empty(&instrumentor_impl->list))
        list_del(&instrumentor_impl->list);
    
    instrumentor_indirect_impl_finalize(&instrumentor_impl->base);
}

static int
instrumentor_indirect_impl_restore_foreign_operations_nodata(
    struct foreign_instrumentor_indirect_impl* instrumentor_impl,
    const void* prototype_object,
    void* object,
    size_t operation_offset,
    void** op_orig)
{
    unsigned long flags;
    int result;
    struct instrumentor_indirect_ops_data* ops_data;
    const void** object_ops_p;
    
    struct instrumentor_indirect_with_foreign_impl*
        instrumentor_impl_binded = instrumentor_impl->instrumentor_impl_binded;

    object_ops_p = indirect_operations_p(object,
        instrumentor_impl_binded->base.operations_field_offset);
    
    spin_lock_irqsave(&instrumentor_impl->base.ops_lock, flags);
    
    ops_data = instrumentor_indirect_impl_find_ops_data(
        &instrumentor_impl->base,
        *object_ops_p);
    
    if(ops_data != NULL)
    {
        *object_ops_p = ops_data->ops_orig;
        *op_orig = operation_at_offset(*object_ops_p, operation_offset);
        result = 0;
    }
    else
    {
        /*
         *  Maybe object operations has changed since
         * call of the replacement operation?
         */
        const void* object_ops = indirect_operations(object,
            instrumentor_impl_binded->base.operations_field_offset);

        *op_orig = instrumentor_indirect_impl_get_operation_not_replaced(
            &instrumentor_impl->base,
            object_ops,
            operation_offset);
        
        result = IS_ERR(*op_orig) ? PTR_ERR(*op_orig) : 0;
    }
    spin_unlock_irqrestore(&instrumentor_impl->base.ops_lock, flags);
    
    return result;
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
    struct foreign_instrumentor_indirect_impl* instrumentor_impl =
        container_of(if_iface_impl, typeof(*instrumentor_impl), base.base);
    
    return instrumentor_indirect_impl_restore_foreign_operations_nodata(
        instrumentor_impl,
        prototype_object,
        object,
        operation_offset,
        op_orig);
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

    object_ops_p = indirect_operations_p(
        object,
        instrumentor_impl->base.operations_field_offset);
    
    instrumentor_indirect_impl_restore_ops(prototype_data_real->ops_data,
        object_ops_p);
    
    *op_orig = operation_at_offset(*object_ops_p, operation_offset);
    
    return 0;
}

static void instrumentor_indirect_impl_if_iface_destroy_impl(
    struct kedr_coi_object* i_iface_impl)
{
    struct foreign_instrumentor_indirect_impl* impl_real =
        container_of(i_iface_impl, typeof(*impl_real), base.base);
    
    foreign_instrumentor_indirect_impl_finalize(impl_real);
    
    kfree(impl_real);
}

static struct kedr_coi_foreign_instrumentor_impl_iface
instrumentor_indirect_impl_if_iface =
{
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

static void instrumentor_indirect_with_foreign_impl_finalize(
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
    
    instrumentor_indirect_impl_finalize(&instrumentor_impl->base);
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
    const void** object_ops_p = indirect_operations_p(
        object,
        instrumentor_impl->operations_field_offset);

    instrumentor_indirect_impl_restore_ops(
        prototype_data_impl->ops_data,
        object_ops_p);
    
    return instrumentor_indirect_impl_replace_operations(
        instrumentor_impl,
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
    const void** object_ops_p = indirect_operations_p(
        object,
        instrumentor_impl->operations_field_offset);

    instrumentor_indirect_impl_restore_ops(
        prototype_data_impl->ops_data,
        object_ops_p);
    
    return instrumentor_indirect_impl_update_operations(
        instrumentor_impl,
        object_data,
        object);
}

static int instrumentor_indirect_impl_chain_operation(
    struct instrumentor_indirect_impl* instrumentor_impl,
    struct instrumentor_indirect_impl* foreign_instrumentor_impl,
    void* object,
    size_t operation_offset,
    void** op_chained)
{
    int result;
    unsigned long flags;
    struct instrumentor_indirect_ops_data* foreign_ops_data;
    
    const void** object_ops_p;
    
    object_ops_p = indirect_operations_p(
        object,
        instrumentor_impl->operations_field_offset);
    
    spin_lock_irqsave(&foreign_instrumentor_impl->ops_lock, flags);
    foreign_ops_data = instrumentor_indirect_impl_find_ops_data(
        foreign_instrumentor_impl,
        *object_ops_p);
    
    if((foreign_ops_data != NULL))
    {
        // Restore copy of operations
        instrumentor_indirect_impl_restore_ops(
            foreign_ops_data,
            object_ops_p);
        *op_chained = operation_at_offset(*object_ops_p, operation_offset);

        result = 0;
    }
    else
    {
        *op_chained = instrumentor_indirect_impl_get_operation_not_replaced(
            instrumentor_impl,
            *object_ops_p,
            operation_offset);
        
        result = IS_ERR(*op_chained) ? PTR_ERR(*op_chained) : 0;
    }

    spin_unlock_irqrestore(&foreign_instrumentor_impl->ops_lock, flags);
        
    return result;
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
    
    *op_repl = operation_at_offset(
        indirect_operations(
            object,
            instrumentor_impl->base.operations_field_offset),
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
    
    *op_repl = operation_at_offset(
        indirect_operations(
            object,
            instrumentor_impl->base.operations_field_offset),
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

    result = instrumentor_indirect_impl_chain_operation(
        &instrumentor_impl->base,
        &foreign_instrumentor_impl->base,
        object,
        operation_offset,
        op_repl);
    
    return result;
}


static void instrumentor_indirect_impl_iwf_iface_destroy_impl(
    struct kedr_coi_object* iwf_iface_impl)
{
    struct instrumentor_indirect_with_foreign_impl* impl_real =
        container_of(iwf_iface_impl, typeof(*impl_real), base.base);
    
    instrumentor_indirect_with_foreign_impl_finalize(impl_real);
    
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
static struct kedr_coi_instrumentor_with_foreign*
instrumentor_indirect_with_foreign_create(
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
        instrumentor_indirect_with_foreign_impl_finalize(instrumentor_impl);
        kfree(instrumentor_impl);
        return NULL;
    }

    return instrumentor;
}

/**********************************************************************/
/************Advanced instrumentor for indirect operation**************/
/**********************************************************************/
/*
 *  Implementation of indirect instrumentor with replacing operations
 *  inside structure instead of replacing whole operations structure.
 */

/*
 * Global map of operations structures which is changed by
 * interception mechanizm.
 * 
 * Need to prevent to interceptors to badly affect on one another.
 */

/*
 * Add pointer to the operations struct(as key) to the map of
 * instrumented operations.
 * 
 * If field already in the map, return -EBUSY.
 */

static int indirect_operations_instrumented_add_unique(
    struct kedr_coi_hash_elem* operations_elem);

static void indirect_operations_instrumented_remove(
    struct kedr_coi_hash_elem* operations_elem);


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
 * except 'refs' field and 'object_ops_elem'.
 */
struct instrumentor_indirect1_ops_data
{
    // hash table organization in instrumentor implementation
    struct kedr_coi_hash_elem object_ops_elem;
    
    void* object_ops;// Pointer to the object operations itself

    void* ops_orig;// Allocated operations structure with original operations.
    
    int refs;
    // Use for global map of indirect operations
    struct kedr_coi_hash_elem operations_elem;
};

/*
 *  Per-object data for instrumentor.
 * 
 * Contain reference to per-operations data plus key for global map
 * of instrumented operations fields.
 */
struct instrumentor_indirect1_object_data
{
    struct kedr_coi_instrumentor_object_data base;
    
    struct instrumentor_indirect1_ops_data* ops_data;
    
    struct kedr_coi_hash_elem operations_field_elem;
};

/*
 * Implementation of advanced indirect instrumentor.
 */
struct instrumentor_indirect1_impl
{
    struct kedr_coi_object base;
    
    struct kedr_coi_hash_table hash_table_ops;
    /*
     * Protect table of per-operations data from concurrent access.
     * Also protect reference count of per-operations data instance
     * from concurrent access.
     */
    spinlock_t ops_lock;
    
    size_t operations_field_offset;
    size_t operations_struct_size;
    
    const struct kedr_coi_replacement* replacements;
};

/*
 * Initialize instance of implementation of
 * advanced indirect instrumentor.
 */
static int instrumentor_indirect1_impl_init(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result = kedr_coi_hash_table_init(&instrumentor_impl->hash_table_ops);
    if(result)
    {
        pr_err("Failed to initialize hash table for operations");
        return result;
    }
    
    instrumentor_impl->operations_struct_size = operations_struct_size;
    instrumentor_impl->operations_field_offset = operations_field_offset;
    
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
    struct kedr_coi_hash_elem* ops_elem =
        kedr_coi_hash_table_find_elem(
            &instrumentor_impl->hash_table_ops,
            ops);
    if(ops_elem == NULL) return NULL;
    return container_of(ops_elem, struct instrumentor_indirect1_ops_data, object_ops_elem);
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
    const void* ops)
{
    const struct kedr_coi_replacement* replacement;
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
    
    kedr_coi_hash_elem_init(&ops_data->operations_elem, ops);
    
    result = indirect_operations_instrumented_add_unique(
        &ops_data->operations_elem);
    switch(result)
    {
    case 0:
        break;
    case -EBUSY:
        pr_err("Failed to watch for an object because its operations "
            "struct already watched by another interceptor.");
    default:
        kfree(ops_data->ops_orig);
        kfree(ops_data);
        return NULL;
    }
    
    kedr_coi_hash_elem_init(&ops_data->object_ops_elem, ops);
    
    kedr_coi_hash_table_add_elem(&instrumentor_impl->hash_table_ops,
        &ops_data->object_ops_elem);
    
    /*
     * Type cast because operations should be changed
     * inside 'const void*' structure.
     */
    ops_data->object_ops = (void*)ops;
    
    memcpy(ops_data->ops_orig, ops,
        instrumentor_impl->operations_struct_size);
    
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        *operation_at_offset_p((void*)ops, replacement->operation_offset) =
            replacement->repl;
    }
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
    const struct kedr_coi_replacement* replacement;

    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        *operation_at_offset_p(ops_data->object_ops, replacement->operation_offset) =
            operation_at_offset(ops_data->ops_orig, replacement->operation_offset);
    }

    kedr_coi_hash_table_remove_elem(&instrumentor_impl->hash_table_ops,
        &ops_data->object_ops_elem);

    indirect_operations_instrumented_remove(&ops_data->operations_elem);

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
    const void* object_ops)
{
    struct instrumentor_indirect1_ops_data* ops_data;
    
    // Look for existed per-operations data for that operations
    ops_data = instrumentor_indirect1_impl_find_ops_data(
        instrumentor_impl,
        object_ops);
    
    if(ops_data)
    {
        instrumentor_indirect1_impl_ref_ops_data(
            instrumentor_impl, ops_data);
        return ops_data;
    }

    ops_data = instrumentor_indirect1_impl_add_ops_data(
        instrumentor_impl, object_ops);

    if(ops_data == NULL) return NULL;
    
    return ops_data;
}

/*
 * This helper is useful for the cases when we doesn't have information
 * about some operations structure.
 * 
 * Helper check whether operation at the given offset is not our
 * replacement operation. It it is so, return that operation.
 * Otherwise ERR_PTR() is returned.
 * 
 * Should be executed under lock for object operations do not changed.
 * NOTE: object lock is not enough for this purpose.
 */
static void* instrumentor_indirect1_impl_get_operation_not_replaced(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    const void* object_ops,
    size_t operation_offset)
{
    const struct kedr_coi_replacement* replacement;
    
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        if(replacement->operation_offset == operation_offset)
        {
            void* op = operation_at_offset(object_ops, operation_offset);
            if(replacement->repl != op)
            {
                return op;
            }
            // Cannot determine original operation
            return ERR_PTR(-ENOENT);
        }
    }
    // operation is not replaced by us
    return operation_at_offset(object_ops, operation_offset);
}

//*********Low-level API of advanced indirect instrumentor************//

static int instrumentor_indirect1_impl_replace_operations(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    struct instrumentor_indirect1_object_data* object_data_new,
    void* object)
{
    unsigned long flags;
    int result;
    struct instrumentor_indirect1_ops_data* ops_data;
    
    const void** object_ops_p = indirect_operations_p(object,
        instrumentor_impl->operations_field_offset);

    kedr_coi_hash_elem_init(&object_data_new->operations_field_elem,
        object_ops_p);
    
    result = indirect_operations_fields_instrumented_add_unique(
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
    default:
        return result;
    }

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    ops_data = instrumentor_indirect1_impl_get_ops_data(
        instrumentor_impl,
        *object_ops_p);
    
    if(ops_data == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        indirect_operations_fields_instrumented_remove(
            &object_data_new->operations_field_elem);
        return -ENOMEM;
    }
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);

    object_data_new->ops_data = ops_data;
    return 0;
}

static int instrumentor_indirect1_impl_update_operations(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    struct instrumentor_indirect1_object_data* object_data,
    void* object)
{
    unsigned long flags;
    struct instrumentor_indirect1_ops_data* ops_data;
    struct instrumentor_indirect1_ops_data* ops_data_new;
    
    const void** object_ops_p = indirect_operations_p(object,
        instrumentor_impl->operations_field_offset);

    ops_data = object_data->ops_data;
    
    if(*object_ops_p == ops_data->object_ops)
        return 0;

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    ops_data_new = instrumentor_indirect1_impl_get_ops_data(
        instrumentor_impl,
        *object_ops_p);
    
    if(ops_data_new == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        return -ENOMEM;
    }
    
    object_data->ops_data = ops_data_new;
    instrumentor_indirect1_impl_unref_ops_data(instrumentor_impl, ops_data);

    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return 0;
}

// 'nodata' parameter has no effect, so it is not used
static void instrumentor_indirect1_impl_clean_replacement(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    struct instrumentor_indirect1_object_data* object_data,
    void* object)
{
    unsigned long flags;
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    instrumentor_indirect1_impl_unref_ops_data(instrumentor_impl,
        object_data->ops_data);
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    indirect_operations_fields_instrumented_remove(
        &object_data->operations_field_elem);
}

static void* instrumentor_indirect1_impl_get_orig_operation_nodata(
    struct instrumentor_indirect1_impl* instrumentor_impl,
    const void* object,
    size_t operation_offset)
{
    unsigned long flags;
    void* op_orig;
    struct instrumentor_indirect1_ops_data* ops_data;
    const void* object_ops = indirect_operations(object,
        instrumentor_impl->operations_field_offset);
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    ops_data = instrumentor_indirect1_impl_find_ops_data(instrumentor_impl,
        object_ops);
    
    if(ops_data != NULL)
    {
        op_orig = operation_at_offset(ops_data->ops_orig, operation_offset);
    }
    else
    {
        op_orig = instrumentor_indirect1_impl_get_operation_not_replaced(
            instrumentor_impl,
            object,
            operation_offset);
    }
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    
    return op_orig;
}


//************Interface of advance indirect instrumentor***************//
static struct kedr_coi_instrumentor_object_data*
instrumentor_indirect1_impl_i_iface_alloc_object_data(
    struct kedr_coi_object* i_iface_impl)
{
    struct instrumentor_indirect1_object_data* object_data =
        kmalloc(sizeof(*object_data), GFP_KERNEL);
    if(object_data == NULL) return NULL;
    
    return &object_data->base;
}

static void
instrumentor_indirect1_impl_i_iface_free_object_data(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    struct instrumentor_indirect1_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    kfree(object_data_real);
}


static int instrumentor_indirect1_impl_i_iface_replace_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object)
{
    struct instrumentor_indirect1_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_object_data* object_data_new_real =
        container_of(object_data_new, typeof(*object_data_new_real), base);
    
    return instrumentor_indirect1_impl_replace_operations(
        instrumentor_impl,
        object_data_new_real,
        object);
}

static int instrumentor_indirect1_impl_i_iface_update_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct instrumentor_indirect1_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    return instrumentor_indirect1_impl_update_operations(
        instrumentor_impl,
        object_data_real,
        object);
}

static void instrumentor_indirect1_impl_i_iface_clean_replacement(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object,
    int norestore)
{
    struct instrumentor_indirect1_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    instrumentor_indirect1_impl_clean_replacement(
        instrumentor_impl,
        object_data_real,
        object);
}



static void instrumentor_indirect1_impl_in_iface_destroy_impl(
    struct kedr_coi_object* in_iface_impl)
{
    struct instrumentor_indirect1_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);

    instrumentor_indirect1_impl_finalize(instrumentor_impl);

    kfree(instrumentor_impl);
}

static void* instrumentor_indirect1_impl_in_iface_get_orig_operation(
    struct kedr_coi_object* in_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    const void* object,
    size_t operation_offset)
{
    struct instrumentor_indirect1_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);

    return operation_at_offset(object_data_real->ops_data->ops_orig,
        operation_offset);
}
    

static void* instrumentor_indirect1_impl_in_iface_get_orig_operation_nodata(
    struct kedr_coi_object* in_iface_impl,
    const void* object,
    size_t operation_offset)
{
    struct instrumentor_indirect1_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_indirect1_impl_get_orig_operation_nodata(
        instrumentor_impl,
        object,
        operation_offset);
}

    
static struct kedr_coi_instrumentor_normal_impl_iface
instrumentor_indirect1_impl_in_iface =
{
    .i_iface =
    {
        .alloc_object_data = instrumentor_indirect1_impl_i_iface_alloc_object_data,
        .free_object_data = instrumentor_indirect1_impl_i_iface_free_object_data,
        
        .replace_operations = instrumentor_indirect1_impl_i_iface_replace_operations,
        .clean_replacement = instrumentor_indirect1_impl_i_iface_clean_replacement,
        .update_operations = instrumentor_indirect1_impl_i_iface_update_operations,
        
        .destroy_impl = instrumentor_indirect1_impl_in_iface_destroy_impl
    },
    
    .get_orig_operation = instrumentor_indirect1_impl_in_iface_get_orig_operation,
    .get_orig_operation_nodata = instrumentor_indirect1_impl_in_iface_get_orig_operation_nodata
};

// Constructor for advanced indirect instrumentor
struct kedr_coi_instrumentor_normal*
instrumentor_indirect1_create(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    struct kedr_coi_instrumentor_normal* instrumentor;
    
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
        operations_field_offset,
        operations_struct_size,
        replacements);
    if(result)
    {
        kfree(instrumentor_impl);
        return NULL;
    }

    instrumentor_impl->base.interface = &instrumentor_indirect1_impl_in_iface.i_iface.base;
    
    instrumentor = kedr_coi_instrumentor_normal_create(&instrumentor_impl->base);
    
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
struct instrumentor_indirect1_with_foreign_ops_data
{
    /*
     *  Current object operations.
     * 
     * After instance creation, this pointer is constant, but operations
     * themselves are not. Only atomic changing of one operation is
     * garanteed (this is need for calling operations from the kernel.)
     */
    void* object_ops;
    /*
     * Copy of original object operations.
     * 
     * After instance creation this pointer and its content are constant
     * (final).
     */
    void* ops_orig;
    // Refcount for share per-operation instance between per-object data.
    int refs;
    // List of foreign per-operations data, which share same operations.
    struct list_head foreign_ops_data;
    // Element in the global table of instrumented operations.
    struct kedr_coi_hash_elem operations_elem;
    // Element in the instrumentor's table of per-operations data.
    struct kedr_coi_hash_elem object_ops_elem;
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
 * 'ops_data_binded' and 'replacements' fields are final after 
 * instance of per-operations data is created.
 */
struct foreign_instrumentor_indirect1_ops_data
{
    // Refcount for share these data inside foreign instrumentor.
    int refs;
    /*
     *  Element in the foreing per-operation data list in the normal
     * per-operations data, which connected with this operations.
     */
    struct list_head list;
    // Pointer to the normal per-operations data.
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data_binded;
    // Replacements of the foreign instrumentor
    const struct kedr_coi_replacement* replacements;
    // Element of the instrumentor's table of per-operations data.
    struct kedr_coi_hash_elem object_ops_elem;
};

/*
 *  Per-object data of the advance indirect instrumentor
 * with foreign support.
 */
struct instrumentor_indirect1_with_foreign_object_data
{
    struct kedr_coi_instrumentor_object_data base;
    
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data;
    
    struct kedr_coi_hash_elem operations_field_elem;
};

// Per-object data of the advance indirect foreign instrumentor.
struct foreign_instrumentor_indirect1_object_data
{
    struct kedr_coi_instrumentor_object_data base;
    
    struct foreign_instrumentor_indirect1_ops_data* ops_data;
    
    struct kedr_coi_hash_elem operations_field_elem;
};

/*
 * Implementation of the advance indirect instrumentor
 * with foreign support.
 */
struct instrumentor_indirect1_with_foreign_impl
{
    struct kedr_coi_object base;
    
    struct kedr_coi_hash_table hash_table_ops;
    /*
     * Protect table of per-operations data from concurrent access.
     * Protect refcount of per-operations data from concurrent access.
     * Protect list of foreign per-operations data within normal
     * per-operations data from concurrent access.
     */
    spinlock_t ops_lock;
    
    size_t operations_field_offset;
    size_t operations_struct_size;
    
    const struct kedr_coi_replacement* replacements;
};

// Initialize instance of the instrumentor.
static int instrumentor_indirect1_with_foreign_impl_init(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result = kedr_coi_hash_table_init(
        &instrumentor_impl->hash_table_ops);
    
    if(result) return result;
    
    instrumentor_impl->operations_field_offset = operations_field_offset;
    instrumentor_impl->operations_struct_size = operations_struct_size;
    instrumentor_impl->replacements = replacements;
    
    spin_lock_init(&instrumentor_impl->ops_lock);
    
    return 0;
}

// Destroy instance of the instrumentor.
// (Instance itself does not freed.)
static void instrumentor_indirect1_with_foreign_impl_finalize(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl)
{
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops, NULL);
}

/*
 *  Search per-operations data for given operations.
 * 
 * Should be executed with lock taken.
 */
static struct instrumentor_indirect1_with_foreign_ops_data*
instrumentor_indirect1_with_foreign_impl_find_ops_data(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    const void* ops)
{
    struct kedr_coi_hash_elem* ops_data_elem =
        kedr_coi_hash_table_find_elem(
            &instrumentor_impl->hash_table_ops,
            ops);
    
    if(ops_data_elem == NULL) return NULL;
    
    return container_of(ops_data_elem,
        struct instrumentor_indirect1_with_foreign_ops_data,
        object_ops_elem);
}

/*
 * Allocate instance of per-operations data, fill its fields and
 * add to the hash tables of operations(instrumentor-local and global ones).
 * 
 * NOTE: This function also replace operations in 'ops'.
 * 
 * Should be executed with lock taken.
 */

static struct instrumentor_indirect1_with_foreign_ops_data*
instrumentor_indirect1_with_foreign_impl_add_ops_data(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    const void* ops)
{
    const struct kedr_coi_replacement* replacement;
    int result;

    struct instrumentor_indirect1_with_foreign_ops_data* ops_data =
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
    
    kedr_coi_hash_elem_init(&ops_data->operations_elem, ops);
    
    result = indirect_operations_instrumented_add_unique(
        &ops_data->operations_elem);
    switch(result)
    {
    case 0:
        break;
    case -EBUSY:
        pr_err("Failed to watch for an object because its operations "
            "struct already watched by another interceptor.");
    default:
        kfree(ops_data->ops_orig);
        kfree(ops_data);
        return NULL;
    }
    
    kedr_coi_hash_elem_init(&ops_data->object_ops_elem, ops);
    
    kedr_coi_hash_table_add_elem(&instrumentor_impl->hash_table_ops,
        &ops_data->object_ops_elem);
    
    /*
     * Type cast because operations should be changed
     * inside 'const void*' structure.
     */
    ops_data->object_ops = (void*)ops;
    
    memcpy(ops_data->ops_orig, ops,
        instrumentor_impl->operations_struct_size);
    
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        *operation_at_offset_p((void*)ops, replacement->operation_offset) =
            replacement->repl;
    }

    return ops_data;
}

/*
 * Destroy content of instance of per-operation data
 * and free instance itself.
 * 
 * Should be executed with lock taken.
 */
static void
instrumentor_indirect1_with_foreign_impl_remove_ops_data(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data)
{
    const struct kedr_coi_replacement* replacement;

    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        *operation_at_offset_p(ops_data->object_ops, replacement->operation_offset) =
            operation_at_offset(ops_data->ops_orig, replacement->operation_offset);
    }

    kedr_coi_hash_table_remove_elem(&instrumentor_impl->hash_table_ops,
        &ops_data->object_ops_elem);

    indirect_operations_instrumented_remove(&ops_data->operations_elem);

    kfree(ops_data->ops_orig);
    kfree(ops_data);
}

/*
 * Increment reference count of per-operations data.
 * 
 * Should be executed with lock taken.
 */
static void
instrumentor_indirect1_with_foreign_impl_ref_ops_data(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data)
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
instrumentor_indirect1_with_foreign_impl_unref_ops_data(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data)
{
    if(--ops_data->refs > 0) return;

    instrumentor_indirect1_with_foreign_impl_remove_ops_data(
        instrumentor_impl,
        ops_data);
}

/*
 * Return per-operations data which corresponds to the given operations
 * and 'ref' these data.
 * 
 * If such data don't exist, create new one and return it.
 * 
 * Should be executed with lock taken.
 */
static struct instrumentor_indirect1_with_foreign_ops_data*
instrumentor_indirect1_with_foreign_impl_get_ops_data(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    const void* ops)
{
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data;
    
    ops_data = instrumentor_indirect1_with_foreign_impl_find_ops_data(
        instrumentor_impl,
        ops);
    
    if(ops_data != NULL)
    {
        instrumentor_indirect1_with_foreign_impl_ref_ops_data(
            instrumentor_impl,
            ops_data);
    }
    else
    {
        ops_data = instrumentor_indirect1_with_foreign_impl_add_ops_data(
            instrumentor_impl,
            ops);
    }
    
    return ops_data;
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
instrumentor_indirect1_with_foreign_impl_get_repl_operation_common(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data,
    const void* object,
    size_t operation_offset)
{
    const struct kedr_coi_replacement* replacement;

    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        if(replacement->operation_offset == operation_offset)
        {
            return replacement->repl;
        }
    }
    
    return operation_at_offset(ops_data->ops_orig, operation_offset);
}

// Implementation of the advance indirect foreign instrumentor.
struct foreign_instrumentor_indirect1_impl
{
    struct kedr_coi_object base;
    
    struct kedr_coi_hash_table hash_table_ops;
    /*
     * Protect table of per-operations data from concurrent access.
     * Protect refs field of per-operations data from concurrent access.
     */
    spinlock_t ops_lock;
    
    size_t operations_field_offset;
    
    const struct kedr_coi_replacement* replacements;
    
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl_binded;
};

/*
 *  Initialize instance of advanced indirect foreign instrumentor
 * implementation.
 */
static int foreign_instrumentor_indirect1_impl_init(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl_binded,
    size_t operations_field_offset,
    const struct kedr_coi_replacement* replacements)
{
    int result = kedr_coi_hash_table_init(
        &instrumentor_impl->hash_table_ops);
    
    if(result) return result;

    instrumentor_impl->operations_field_offset = operations_field_offset;
    instrumentor_impl->replacements = replacements;
    instrumentor_impl->instrumentor_impl_binded = instrumentor_impl_binded;
    
    spin_lock_init(&instrumentor_impl->ops_lock);
    
    return 0;
}

/*
 *  Destroy instance of advanced indirect foreign instrumentor
 * implementation. (Instance itself do not freed.)
 */
static void foreign_instrumentor_indirect1_impl_finalize(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl)
{
    kedr_coi_hash_table_destroy(&instrumentor_impl->hash_table_ops, NULL);
}

/*
 * Search per-operations data.
 * 
 * Should be executed with lock taken.
 */
static struct foreign_instrumentor_indirect1_ops_data*
foreign_instrumentor_indirect1_impl_find_ops_data(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl,
    const void* ops)
{
    struct kedr_coi_hash_elem* ops_data_elem =
        kedr_coi_hash_table_find_elem(
            &instrumentor_impl->hash_table_ops,
            ops);
    
    if(ops_data_elem == NULL) return NULL;
    
    return container_of(ops_data_elem,
        struct foreign_instrumentor_indirect1_ops_data,
        object_ops_elem);

}

/*
 * Allocate instance of per-operations data, fill its fields and
 * add to the hash table of operations(instrumentor-local).
 * 
 * NOTE: This function also replace operations in 'ops'.
 * 
 * Should be executed with both locks taken - for foreign operations and
 * for normal ones.
 */
static struct foreign_instrumentor_indirect1_ops_data*
foreign_instrumentor_indirect1_impl_add_ops_data(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl,
    const void* ops)
{
    struct instrumentor_indirect1_with_foreign_impl*
        instrumentor_impl_binded = instrumentor_impl->instrumentor_impl_binded;
    
    const struct kedr_coi_replacement* replacement;

    struct foreign_instrumentor_indirect1_ops_data* ops_data;
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data_binded;

    ops_data_binded = instrumentor_indirect1_with_foreign_impl_get_ops_data(
        instrumentor_impl_binded, ops);
    
    if(ops_data_binded == NULL) return NULL;
    
    ops_data = kmalloc(sizeof(*ops_data), GFP_ATOMIC);
    
    if(ops_data == NULL)
    {
        pr_err("Failed to allocate operations data for foreign instrumentor.");
        instrumentor_indirect1_with_foreign_impl_unref_ops_data(
            instrumentor_impl_binded, ops_data_binded);
        return NULL;
    }
    
    INIT_LIST_HEAD(&ops_data->list);
    ops_data->replacements = instrumentor_impl->replacements;
    
    kedr_coi_hash_elem_init(&ops_data->object_ops_elem, ops);
    
    kedr_coi_hash_table_add_elem(&instrumentor_impl->hash_table_ops,
        &ops_data->object_ops_elem);
    
    list_add_tail(&ops_data->list, &ops_data_binded->foreign_ops_data);
    ops_data->ops_data_binded = ops_data_binded;
    
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        *operation_at_offset_p((void*)ops, replacement->operation_offset) =
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
 * 'ops_data_binded' are normal per-operations data corresponded to
 * deleted one.
 * 
 * 'ops_data_next' is the foreign per-operations data which comes after
 * deleted one in the list of 'ops_data_binded'.
 * 
 * Should be executed with both locks taken - for foreign operations and
 * for normal ones.
 */
static void foreign_instrumentor_indirect1_impl_rollback_replacement(
    const struct kedr_coi_replacement* replacement,
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data_binded,
    struct foreign_instrumentor_indirect1_ops_data* ops_data_next)
{
    void** op = operation_at_offset_p(ops_data_binded->object_ops,
        replacement->operation_offset);
    if(*op != replacement->repl) return;

    list_for_each_entry_continue_reverse(ops_data_next,
        &ops_data_binded->foreign_ops_data, list)
    {
        const struct kedr_coi_replacement* replacement_tmp;
        for(replacement_tmp = ops_data_next->replacements;
            replacement_tmp->operation_offset != -1;
            replacement_tmp++)
        {
            if(replacement_tmp->operation_offset == replacement->operation_offset)
            {
                *op = replacement_tmp->repl;
                return;
            }
        }
    }
    *op = operation_at_offset(ops_data_binded->ops_orig, replacement->operation_offset);
}

/*
 * Destroy instance of foreign per-operations data and
 * free instance itself.
 * 
 * Also unref normal per-operations data binded with removing one.
 *
 * Should be executed with both locks taken - for foreign operations and
 * for normal ones.
 */
static void
foreign_instrumentor_indirect1_impl_remove_ops_data(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl,
    struct foreign_instrumentor_indirect1_ops_data* ops_data)
{
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data_binded =
        ops_data->ops_data_binded;
    const struct kedr_coi_replacement* replacement;
    struct foreign_instrumentor_indirect1_ops_data* ops_data_next =
        list_entry(ops_data->list.next, typeof(*ops_data), list);

    list_del(&ops_data->list);
    
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        foreign_instrumentor_indirect1_impl_rollback_replacement(
            replacement,
            ops_data_binded,
            ops_data_next);
    }
       
    kedr_coi_hash_table_remove_elem(&instrumentor_impl->hash_table_ops,
        &ops_data->object_ops_elem);

    kfree(ops_data);
    
    instrumentor_indirect1_with_foreign_impl_unref_ops_data(
        instrumentor_impl->instrumentor_impl_binded,
        ops_data_binded);
}

/*
 *  Increment reference count of foreign per-operations data.
 * 
 * Should be executed with lock taken.
 */

static void
foreign_instrumentor_indirect1_impl_ref_ops_data(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl,
    struct foreign_instrumentor_indirect1_ops_data* ops_data)
{
    ops_data->refs++;
}

/*
 * Decrement reference count of foreign per-operations data.
 * 
 * If it become 0, remove them.
 * 
 * Should be executed with both locks taken - for foreign operations and
 * for normal ones.
 */
static void
foreign_instrumentor_indirect1_impl_unref_ops_data(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl,
    struct foreign_instrumentor_indirect1_ops_data* ops_data)
{
    if(--ops_data->refs > 0) return;

    foreign_instrumentor_indirect1_impl_remove_ops_data(
        instrumentor_impl,
        ops_data);
}

/*
 * Return per-operations data which corresponds to the given operations
 * and 'ref' these data.
 * 
 * If such data don't exist, create new one and return it.
 *
 * Should be executed with both locks taken - for foreign operations and
 * for normal ones.
 */
static struct foreign_instrumentor_indirect1_ops_data*
foreign_instrumentor_indirect1_impl_get_ops_data(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl,
    const void* ops)
{
    struct foreign_instrumentor_indirect1_ops_data* ops_data;
    
    ops_data = foreign_instrumentor_indirect1_impl_find_ops_data(
        instrumentor_impl,
        ops);
    
    if(ops_data != NULL)
    {
        foreign_instrumentor_indirect1_impl_ref_ops_data(
            instrumentor_impl,
            ops_data);
    }
    else
    {
        ops_data = foreign_instrumentor_indirect1_impl_add_ops_data(
            instrumentor_impl,
            ops);
    }
    
    return ops_data;
}

//**** Low level API of advanced foreign indirect instrumentor ****//
static int foreign_instrumentor_indirect1_impl_replace_operations(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl,
    struct foreign_instrumentor_indirect1_object_data* object_data_new,
    void* object)
{
    unsigned long flags;
    unsigned long flags_binded;
    
    int result;
    struct foreign_instrumentor_indirect1_ops_data* ops_data;
    
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl_binded =
        instrumentor_impl->instrumentor_impl_binded;
    
    const void** object_ops_p = indirect_operations_p(object,
        instrumentor_impl->operations_field_offset);

    kedr_coi_hash_elem_init(&object_data_new->operations_field_elem,
        object_ops_p);
    
    result = indirect_operations_fields_instrumented_add_unique(
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
    default:
        return result;
    }

    spin_lock_irqsave(&instrumentor_impl_binded->ops_lock, flags_binded);
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    ops_data = foreign_instrumentor_indirect1_impl_get_ops_data(
        instrumentor_impl,
        *object_ops_p);
    
    if(ops_data == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl_binded->ops_lock, flags_binded);
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        indirect_operations_fields_instrumented_remove(&object_data_new->operations_field_elem);
        return -ENOMEM;
    }
    
    spin_unlock_irqrestore(&instrumentor_impl_binded->ops_lock, flags_binded);
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);

    object_data_new->ops_data = ops_data;
    
    return 0;
}

static int foreign_instrumentor_indirect1_impl_update_operations(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl,
    struct foreign_instrumentor_indirect1_object_data* object_data,
    void* object)
{
    unsigned long flags;
    unsigned long flags_binded;

    struct foreign_instrumentor_indirect1_ops_data* ops_data;
    struct foreign_instrumentor_indirect1_ops_data* ops_data_new;
    
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl_binded =
        instrumentor_impl->instrumentor_impl_binded;
    
    const void** object_ops_p = indirect_operations_p(object,
        instrumentor_impl->operations_field_offset);

    ops_data = object_data->ops_data;
    
    if(*object_ops_p == ops_data->ops_data_binded->object_ops)
        return 0;

    spin_lock_irqsave(&instrumentor_impl_binded->ops_lock, flags_binded);
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    
    ops_data_new = foreign_instrumentor_indirect1_impl_get_ops_data(
        instrumentor_impl,
        *object_ops_p);
    
    if(ops_data_new == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl_binded->ops_lock, flags_binded);
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        return -ENOMEM;
    }

    foreign_instrumentor_indirect1_impl_unref_ops_data(instrumentor_impl, ops_data);
    
    spin_unlock_irqrestore(&instrumentor_impl_binded->ops_lock, flags_binded);
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);

    object_data->ops_data = ops_data_new;
    
    
    return 0;
}


static void foreign_instrumentor_indirect1_impl_clean_replacement(
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl,
    struct foreign_instrumentor_indirect1_object_data* object_data,
    void* object)
{
    unsigned long flags;
    unsigned long flags_binded;
    
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl_binded =
        instrumentor_impl->instrumentor_impl_binded;

    spin_lock_irqsave(&instrumentor_impl_binded->ops_lock, flags_binded);
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);

    foreign_instrumentor_indirect1_impl_unref_ops_data(instrumentor_impl,
        object_data->ops_data);
    
    spin_unlock_irqrestore(&instrumentor_impl_binded->ops_lock, flags_binded);
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);

    
    indirect_operations_fields_instrumented_remove(
        &object_data->operations_field_elem);
}

static void*
instrumentor_indirect1_with_foreign_impl_get_orig_operation_nodata(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    const void* object,
    size_t operation_offset)
{
    unsigned long flags;

    const struct kedr_coi_replacement* replacement;
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data;

    void* op;
    const void* object_ops = indirect_operations(object,
        instrumentor_impl->operations_field_offset);
    
    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);

    ops_data = instrumentor_indirect1_with_foreign_impl_find_ops_data(
        instrumentor_impl,
        object_ops);
    
    if(ops_data != NULL)
    {
        op = operation_at_offset(ops_data->ops_orig,
            operation_offset);
        goto out;
    }
    
    op = operation_at_offset(object_ops, operation_offset);
    
    for(replacement = instrumentor_impl->replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        if(replacement->operation_offset == operation_offset)
        {
            if(op == replacement->repl)
            {
                op = ERR_PTR(-EINVAL);
            }
            goto out;
        }
    }

out:
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    return op;
}

//*Low level API of advanced indirect instrumentor with foreign support*//

/*
 * Same as standard 'replace_operations' but also accept hint, which
 * is probably per-operations data corresponded to the given object.
 * 
 * Should be executed without lock.
 */
static int instrumentor_indirect1_with_foreign_impl_replace_operations(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_object_data* object_data_new,
    void* object,
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data_hint)
{
    unsigned long flags;
    int result;
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data;
    
    const void** object_ops_p = indirect_operations_p(object,
        instrumentor_impl->operations_field_offset);

    kedr_coi_hash_elem_init(&object_data_new->operations_field_elem,
        object_ops_p);
    
    result = indirect_operations_fields_instrumented_add_unique(
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
    default:
        return result;
    }

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    if(ops_data_hint->object_ops == *object_ops_p)
    {
        instrumentor_indirect1_with_foreign_impl_ref_ops_data(
            instrumentor_impl,
            ops_data_hint);

        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        object_data_new->ops_data = ops_data_hint;
        return 0;
    }

    ops_data = instrumentor_indirect1_with_foreign_impl_get_ops_data(
        instrumentor_impl,
        *object_ops_p);


    if(ops_data == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        indirect_operations_fields_instrumented_remove(
            &object_data_new->operations_field_elem);
        return -EINVAL;
    }
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
    object_data_new->ops_data = ops_data;
    
    return 0;
}

static int instrumentor_indirect1_with_foreign_impl_update_operations(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_object_data* object_data,
    void* object)
{
    unsigned long flags;
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data;
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data_new;
    
    const void** object_ops_p = indirect_operations_p(object,
        instrumentor_impl->operations_field_offset);

    ops_data = object_data->ops_data;
    
    if(*object_ops_p == ops_data->object_ops)
        return 0;

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    ops_data_new = instrumentor_indirect1_with_foreign_impl_get_ops_data(
        instrumentor_impl,
        *object_ops_p);
    
    if(ops_data_new == NULL)
    {
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        return -EINVAL;
    }

    instrumentor_indirect1_with_foreign_impl_unref_ops_data(
        instrumentor_impl, ops_data);
    
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);

    object_data->ops_data = ops_data_new;
    return 0;
}


static void instrumentor_indirect1_with_foreign_impl_clean_replacement(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_object_data* object_data,
    void* object)
{
    unsigned long flags;

    spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
    instrumentor_indirect1_with_foreign_impl_unref_ops_data(instrumentor_impl,
        object_data->ops_data);
    spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);

    indirect_operations_fields_instrumented_remove(
        &object_data->operations_field_elem);
}

 

/*
 * Low level API of indirect instrumentor for implementation of
 * 'bind_prototype_with_object'.
 */

static int instrumentor_indirect1_replace_operations_from_prototype(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    struct foreign_instrumentor_indirect1_impl* foreign_instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_object_data* object_data_new,
    void* object,
    struct foreign_instrumentor_indirect1_object_data* prototype_object_data,
    void* prototype_object,
    size_t operation_offset,
    void **op_repl)
{
    int result;
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data_binded =
        prototype_object_data->ops_data->ops_data_binded;

    const void* object_ops = indirect_operations(object,
        instrumentor_impl->operations_field_offset);
    
    result = instrumentor_indirect1_with_foreign_impl_replace_operations(
        instrumentor_impl,
        object_data_new,
        object,
        ops_data_binded);

    if(result)
    {
        if(ops_data_binded->object_ops == object_ops)
        {
            *op_repl = operation_at_offset(
                ops_data_binded->ops_orig, operation_offset);
            return result;
        }

        *op_repl = instrumentor_indirect1_with_foreign_impl_get_orig_operation_nodata(
            instrumentor_impl,
            object,
            operation_offset);
        
        return result;
    }

    *op_repl = instrumentor_indirect1_with_foreign_impl_get_repl_operation_common(
        instrumentor_impl,
        object_data_new->ops_data,
        object,
        operation_offset);
    return 0;
}

static int instrumentor_indirect1_update_operations_from_prototype(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    struct foreign_instrumentor_indirect1_impl* foreign_instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_object_data* object_data,
    void* object,
    struct foreign_instrumentor_indirect1_object_data* prototype_object_data,
    void* prototype_object,
    size_t operation_offset,
    void **op_repl)
{
    int result;
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data_binded =
        prototype_object_data->ops_data->ops_data_binded;
    
    const void* object_ops = indirect_operations(object,
        instrumentor_impl->operations_field_offset);
    
    if(object_data->ops_data->object_ops == object_ops)
    {
        *op_repl = instrumentor_indirect1_with_foreign_impl_get_repl_operation_common(
            instrumentor_impl,
            object_data->ops_data,
            object,
            operation_offset);

        return 0;
    }
    else if(ops_data_binded->object_ops == object_ops)
    {
        unsigned long flags;

        spin_lock_irqsave(&instrumentor_impl->ops_lock, flags);
        instrumentor_indirect1_with_foreign_impl_ref_ops_data(
            instrumentor_impl, ops_data_binded);
        instrumentor_indirect1_with_foreign_impl_unref_ops_data(
            instrumentor_impl, object_data->ops_data);
        spin_unlock_irqrestore(&instrumentor_impl->ops_lock, flags);
        
        object_data->ops_data = ops_data_binded;
        
        *op_repl = instrumentor_indirect1_with_foreign_impl_get_repl_operation_common(
            instrumentor_impl,
            object_data->ops_data,
            object,
            operation_offset);

        return 0;
    }

    result = instrumentor_indirect1_with_foreign_impl_update_operations(
        instrumentor_impl,
        object_data,
        object);
    
    if(result)
    {
        *op_repl = instrumentor_indirect1_with_foreign_impl_get_orig_operation_nodata(
            instrumentor_impl,
            object,
            operation_offset);
        
        return result;
    }

    *op_repl = instrumentor_indirect1_with_foreign_impl_get_repl_operation_common(
        instrumentor_impl,
        object_data->ops_data,
        object,
        operation_offset);

    return 0;
}

static int instrumentor_indirect1_restore_foreign_operations(
    struct foreign_instrumentor_indirect1_impl* foreign_instrumentor_impl,
    struct foreign_instrumentor_indirect1_object_data* prototype_data,
    const void* prototype_object,
    void* object,
    size_t operation_offset,
    void** op_orig)
{
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data_binded =
        prototype_data->ops_data->ops_data_binded;
    
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl_binded =
        foreign_instrumentor_impl->instrumentor_impl_binded;
    
    const void* object_ops = indirect_operations(object,
        instrumentor_impl_binded->operations_field_offset);
    
    if(ops_data_binded->object_ops == object_ops)
    {
        *op_orig = operation_at_offset(ops_data_binded->ops_orig,
            operation_offset);
        return 0;
    }
        
    *op_orig = instrumentor_indirect1_with_foreign_impl_get_orig_operation_nodata(
        instrumentor_impl_binded,
        object,
        operation_offset);

    return IS_ERR(*op_orig) ? PTR_ERR(*op_orig) : 0;
}

static int instrumentor_indirect1_restore_foreign_operations_nodata(
    struct foreign_instrumentor_indirect1_impl* foreign_instrumentor_impl,
    const void* prototype_object,
    void* object,
    size_t operation_offset,
    void** op_orig)
{
    unsigned long flags;
    unsigned long flags_binded;
    
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl_binded =
        foreign_instrumentor_impl->instrumentor_impl_binded;

    struct foreign_instrumentor_indirect1_ops_data* foreign_ops_data;
    struct instrumentor_indirect1_with_foreign_ops_data* ops_data;
    
    const void* object_ops = indirect_operations(object,
        instrumentor_impl_binded->operations_field_offset);
    
    spin_lock_irqsave(&instrumentor_impl_binded->ops_lock, flags_binded);
    spin_lock_irqsave(&foreign_instrumentor_impl->ops_lock, flags);
    
    foreign_ops_data = foreign_instrumentor_indirect1_impl_find_ops_data(
        foreign_instrumentor_impl,
        object_ops);
            
    spin_unlock_irqrestore(&foreign_instrumentor_impl->ops_lock, flags);

    if(foreign_ops_data != NULL)
    {
        // 'foreign_ops_data' cannot be destroyed outside
        // because we hold lock for binded instrumentor
        ops_data = foreign_ops_data->ops_data_binded;
    }
    else
    {
        
        ops_data = instrumentor_indirect1_with_foreign_impl_find_ops_data(
            instrumentor_impl_binded,
            object_ops);
        if(ops_data == NULL) goto no_repl;

        foreign_ops_data = list_prepare_entry(foreign_ops_data,
            &ops_data->foreign_ops_data, list);
    }
    list_for_each_entry_continue_reverse(foreign_ops_data,
        &ops_data->foreign_ops_data,
        list)
    {
        const struct kedr_coi_replacement* replacement;
        for(replacement = foreign_ops_data->replacements;
            replacement->operation_offset != -1;
            replacement++)
        {
            if(replacement->operation_offset == operation_offset)
            {
                *op_orig = replacement->repl;
                spin_unlock_irqrestore(&instrumentor_impl_binded->ops_lock, flags_binded);
                return 0;
            }
        }
    }
    
no_repl:

    spin_unlock_irqrestore(&instrumentor_impl_binded->ops_lock, flags_binded);

    *op_orig = instrumentor_indirect1_with_foreign_impl_get_orig_operation_nodata(
            instrumentor_impl_binded,
            object,
            operation_offset);
    
    
    return IS_ERR(*op_orig) ? PTR_ERR(*op_orig) : 0;
}

static void* instrumentor_indirect1_chain_operation(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl,
    struct instrumentor_indirect1_with_foreign_object_data* object_data,
    const void* prototype_object,
    void* object,
    size_t operation_offset)
{
    return instrumentor_indirect1_with_foreign_impl_get_repl_operation_common(
        instrumentor_impl,
        object_data->ops_data,
        object,
        operation_offset);
}

//** Interface of advanced indirect instrumentor for foreign support **//
static struct kedr_coi_instrumentor_object_data*
instrumentor_indirect1_impl_if_iface_alloc_object_data(
    struct kedr_coi_object* i_iface_impl)
{
    struct foreign_instrumentor_indirect1_object_data* object_data =
        kmalloc(sizeof(*object_data), GFP_KERNEL);
    
    if(object_data == NULL) return NULL;
    
    return &object_data->base;
}
    
static void
instrumentor_indirect1_impl_if_iface_free_object_data(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    struct foreign_instrumentor_indirect1_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    kfree(object_data_real);
}

static int
instrumentor_indirect1_impl_if_iface_replace_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object)
{
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    struct foreign_instrumentor_indirect1_object_data* object_data_new_real =
        container_of(object_data_new, typeof(*object_data_new_real), base);
    
    return foreign_instrumentor_indirect1_impl_replace_operations(
        instrumentor_impl,
        object_data_new_real,
        object);
}

static int
instrumentor_indirect1_impl_if_iface_update_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    struct foreign_instrumentor_indirect1_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    return foreign_instrumentor_indirect1_impl_update_operations(
        instrumentor_impl,
        object_data_real,
        object);
}

static void
instrumentor_indirect1_impl_if_iface_clean_replacement(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object,
    int norestore)
{
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    struct foreign_instrumentor_indirect1_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    foreign_instrumentor_indirect1_impl_clean_replacement(
        instrumentor_impl,
        object_data_real,
        object);
}

static void instrumentor_indirect1_impl_if_iface_destroy_impl(
    struct kedr_coi_object* i_iface_impl)
{
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);

    foreign_instrumentor_indirect1_impl_finalize(instrumentor_impl);
    
    kfree(instrumentor_impl);
}

static int
instrumentor_indirect1_impl_if_iface_restore_foreign_operations(
    struct kedr_coi_object* if_iface_impl,
    struct kedr_coi_instrumentor_object_data* prototype_data,
    const void* prototype_object,
    void* object,
    size_t operation_offset,
    void** op_orig)
{
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl =
        container_of(if_iface_impl, typeof(*instrumentor_impl), base);
    
    struct foreign_instrumentor_indirect1_object_data* prototype_data_real =
        container_of(prototype_data, typeof(*prototype_data_real), base);


    return instrumentor_indirect1_restore_foreign_operations(
        instrumentor_impl,
        prototype_data_real,
        prototype_object,
        object,
        operation_offset,
        op_orig);
}

static int
instrumentor_indirect1_impl_if_iface_restore_foreign_operations_nodata(
    struct kedr_coi_object* if_iface_impl,
    const void* prototype_object,
    void* object,
    size_t operation_offset,
    void** op_orig)
{
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl =
        container_of(if_iface_impl, typeof(*instrumentor_impl), base);
    
    return instrumentor_indirect1_restore_foreign_operations_nodata(
        instrumentor_impl,
        prototype_object,
        object,
        operation_offset,
        op_orig);
}

static struct kedr_coi_foreign_instrumentor_impl_iface
instrumentor_indirect1_impl_if_iface =
{
    .i_iface =
    {
        .alloc_object_data = instrumentor_indirect1_impl_if_iface_alloc_object_data,
        .free_object_data = instrumentor_indirect1_impl_if_iface_free_object_data,
        
        .replace_operations = instrumentor_indirect1_impl_if_iface_replace_operations,
        .update_operations = instrumentor_indirect1_impl_if_iface_update_operations,
        .clean_replacement = instrumentor_indirect1_impl_if_iface_clean_replacement,
        
        .destroy_impl = instrumentor_indirect1_impl_if_iface_destroy_impl
    },
    .restore_foreign_operations =
        instrumentor_indirect1_impl_if_iface_restore_foreign_operations,
    .restore_foreign_operations_nodata =
        instrumentor_indirect1_impl_if_iface_restore_foreign_operations_nodata
};

// Constructor for foreign instrumentor(for internal use)
static struct foreign_instrumentor_indirect1_impl*
foreign_instrumentor_indirect1_impl_create(
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl_binded,
    size_t operations_field_offset,
    const struct kedr_coi_replacement* replacements)
{
    int result;
    struct foreign_instrumentor_indirect1_impl* instrumentor_impl =
        kmalloc(sizeof(*instrumentor_impl), GFP_KERNEL);
    
    if(instrumentor_impl == NULL)
    {
        pr_err("Failed to allocate structure for foreign instrumentor.");
        return NULL;
    }
    
    result = foreign_instrumentor_indirect1_impl_init(instrumentor_impl,
        instrumentor_impl_binded,
        operations_field_offset,
        replacements);
    
    if(result)
    {
        kfree(instrumentor_impl);
        return NULL;
    }
    
    instrumentor_impl->base.interface = &instrumentor_indirect1_impl_if_iface.i_iface.base;
    
    return instrumentor_impl;
}

static struct kedr_coi_instrumentor_object_data*
instrumentor_indirect1_impl_iwf_iface_alloc_object_data(
    struct kedr_coi_object* i_iface_impl)
{
    struct instrumentor_indirect1_with_foreign_object_data* object_data =
        kmalloc(sizeof(*object_data), GFP_KERNEL);
    
    if(object_data == NULL) return NULL;
    
    return &object_data->base;
}
    
static void
instrumentor_indirect1_impl_iwf_iface_free_object_data(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data)
{
    struct instrumentor_indirect1_with_foreign_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    kfree(object_data_real);
}

static int
instrumentor_indirect1_impl_iwf_iface_replace_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object)
{
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_with_foreign_object_data* object_data_new_real =
        container_of(object_data_new, typeof(*object_data_new_real), base);
    
    return instrumentor_indirect1_with_foreign_impl_replace_operations(
        instrumentor_impl,
        object_data_new_real,
        object,
        NULL);
}

static int
instrumentor_indirect1_impl_iwf_iface_update_operations(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object)
{
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_with_foreign_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    return instrumentor_indirect1_with_foreign_impl_update_operations(
        instrumentor_impl,
        object_data_real,
        object);
}

static void
instrumentor_indirect1_impl_iwf_iface_clean_replacement(
    struct kedr_coi_object* i_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object,
    int norestore)
{
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_with_foreign_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);
    
    instrumentor_indirect1_with_foreign_impl_clean_replacement(
        instrumentor_impl,
        object_data_real,
        object);
}

static void instrumentor_indirect1_impl_iwf_iface_destroy_impl(
    struct kedr_coi_object* i_iface_impl)
{
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl =
        container_of(i_iface_impl, typeof(*instrumentor_impl), base);

    instrumentor_indirect1_with_foreign_impl_finalize(instrumentor_impl);
    
    kfree(instrumentor_impl);
}

static void* instrumentor_indirect1_impl_iwf_iface_get_orig_operation(
    struct kedr_coi_object* in_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    const void* object,
    size_t operation_offset)
{
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_with_foreign_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);

    return operation_at_offset(object_data_real->ops_data->ops_orig,
        operation_offset);
}

static void* instrumentor_indirect1_impl_iwf_iface_get_orig_operation_nodata(
    struct kedr_coi_object* in_iface_impl,
    const void* object,
    size_t operation_offset)
{
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl =
        container_of(in_iface_impl, typeof(*instrumentor_impl), base);

    return instrumentor_indirect1_with_foreign_impl_get_orig_operation_nodata(
        instrumentor_impl,
        object,
        operation_offset);
}


static struct kedr_coi_object*
instrumentor_indirect1_impl_iwf_iface_create_foreign_indirect(
    struct kedr_coi_object* iwf_iface_impl,
    size_t operations_field_offset,
    const struct kedr_coi_replacement* replacements)
{
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl =
        container_of(iwf_iface_impl, typeof(*instrumentor_impl), base);
    
    struct foreign_instrumentor_indirect1_impl* foreign_instrumentor_impl =
        foreign_instrumentor_indirect1_impl_create(
            instrumentor_impl,
            operations_field_offset,
            replacements);
    
    if(foreign_instrumentor_impl == NULL) return NULL;
    
    return &foreign_instrumentor_impl->base;
}

static int
instrumentor_indirect1_impl_iwf_iface_replace_operations_from_prototype(
    struct kedr_coi_object* iwf_iface_impl,
    struct kedr_coi_object* if_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data_new,
    void* object,
    struct kedr_coi_instrumentor_object_data* prototype_data,
    void* prototype_object,
    size_t operation_offset,
    void** op_repl)
{
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl =
        container_of(iwf_iface_impl, typeof(*instrumentor_impl), base);
    
    struct foreign_instrumentor_indirect1_impl* foreign_instrumentor_impl =
        container_of(if_iface_impl, typeof(*foreign_instrumentor_impl), base);
    
    struct instrumentor_indirect1_with_foreign_object_data* object_data_new_real =
        container_of(object_data_new, typeof(*object_data_new_real), base);

    struct foreign_instrumentor_indirect1_object_data* prototype_data_real =
        container_of(prototype_data, typeof(*prototype_data_real), base);

    return instrumentor_indirect1_replace_operations_from_prototype(
        instrumentor_impl,
        foreign_instrumentor_impl,
        object_data_new_real,
        object,
        prototype_data_real,
        prototype_object,
        operation_offset,
        op_repl);
}

static int
instrumentor_indirect1_impl_iwf_iface_update_operations_from_prototype(
    struct kedr_coi_object* iwf_iface_impl,
    struct kedr_coi_object* if_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object,
    struct kedr_coi_instrumentor_object_data* prototype_data,
    void* prototype_object,
    size_t operation_offset,
    void** op_repl)
{
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl =
        container_of(iwf_iface_impl, typeof(*instrumentor_impl), base);
    
    struct foreign_instrumentor_indirect1_impl* foreign_instrumentor_impl =
        container_of(if_iface_impl, typeof(*foreign_instrumentor_impl), base);
    
    struct instrumentor_indirect1_with_foreign_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);

    struct foreign_instrumentor_indirect1_object_data* prototype_data_real =
        container_of(prototype_data, typeof(*prototype_data_real), base);

    return instrumentor_indirect1_update_operations_from_prototype(
        instrumentor_impl,
        foreign_instrumentor_impl,
        object_data_real,
        object,
        prototype_data_real,
        prototype_object,
        operation_offset,
        op_repl);
}

static int
instrumentor_indirect1_impl_iwf_iface_chain_operation(
    struct kedr_coi_object* iwf_iface_impl,
    struct kedr_coi_object* if_iface_impl,
    struct kedr_coi_instrumentor_object_data* object_data,
    void* object,
    void* prototype_object,
    size_t operation_offset,
    void** op_chained)
{
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl =
        container_of(iwf_iface_impl, typeof(*instrumentor_impl), base);
    
    struct instrumentor_indirect1_with_foreign_object_data* object_data_real =
        container_of(object_data, typeof(*object_data_real), base);

    *op_chained = instrumentor_indirect1_chain_operation(
        instrumentor_impl,
        object_data_real,
        object,
        prototype_object,
        operation_offset);
    
    return IS_ERR(*op_chained)? PTR_ERR(*op_chained) : 0;
}

static struct kedr_coi_instrumentor_with_foreign_impl_iface
instrumentor_indirect1_impl_iwf_iface =
{
    .in_iface =
    {
        .i_iface =
        {
            .alloc_object_data = instrumentor_indirect1_impl_iwf_iface_alloc_object_data,
            .free_object_data = instrumentor_indirect1_impl_iwf_iface_free_object_data,
            
            .replace_operations = instrumentor_indirect1_impl_iwf_iface_replace_operations,
            .update_operations = instrumentor_indirect1_impl_iwf_iface_update_operations,
            .clean_replacement = instrumentor_indirect1_impl_iwf_iface_clean_replacement,
            
            .destroy_impl = instrumentor_indirect1_impl_iwf_iface_destroy_impl,
        },
        .get_orig_operation = instrumentor_indirect1_impl_iwf_iface_get_orig_operation,
        .get_orig_operation_nodata =
            instrumentor_indirect1_impl_iwf_iface_get_orig_operation_nodata,
    },
    .create_foreign_indirect = instrumentor_indirect1_impl_iwf_iface_create_foreign_indirect,
    .replace_operations_from_prototype =
        instrumentor_indirect1_impl_iwf_iface_replace_operations_from_prototype,
    .update_operations_from_prototype =
        instrumentor_indirect1_impl_iwf_iface_update_operations_from_prototype,
    .chain_operation =
        instrumentor_indirect1_impl_iwf_iface_chain_operation,
};


static struct kedr_coi_instrumentor_with_foreign*
instrumentor_indirect1_with_foreign_create(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    int result;
    struct kedr_coi_instrumentor_with_foreign* instrumentor;
    struct instrumentor_indirect1_with_foreign_impl* instrumentor_impl;
    
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
    
    result = instrumentor_indirect1_with_foreign_impl_init(
        instrumentor_impl,
        operations_field_offset,
        operations_struct_size,
        replacements);
    
    if(result)
    {
        kfree(instrumentor_impl);
        return NULL;
    }
    
    instrumentor_impl->base.interface =
        &instrumentor_indirect1_impl_iwf_iface.in_iface.i_iface.base;
    
    instrumentor = kedr_coi_instrumentor_with_foreign_create(
        &instrumentor_impl->base);
    
    if(instrumentor == NULL)
    {
        instrumentor_indirect1_with_foreign_impl_finalize(instrumentor_impl);
        kfree(instrumentor_impl);
        return NULL;
    }
    
    return instrumentor;
}
//****************Implementation of global maps**********************//

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


//**Implementation of the global map for indirect operations fields***//
static struct kedr_coi_hash_table indirect_operations_fields_instrumented;
DEFINE_SPINLOCK(indirect_operations_fields_instrumented_lock);

int indirect_operations_fields_instrumented_add_unique(
    struct kedr_coi_hash_elem* operations_field_elem)
{
    unsigned long flags;

    //pr_info("Add operations field %p to the table %p.",
    //    operations_field_elem->key, &indirect_operations_fields_instrumented);
    
    spin_lock_irqsave(&indirect_operations_fields_instrumented_lock, flags);
    if(kedr_coi_hash_table_find_elem(&indirect_operations_fields_instrumented,
        operations_field_elem->key))
    {
        spin_unlock_irqrestore(&indirect_operations_fields_instrumented_lock, flags);
        return -EBUSY;
    }
    
    kedr_coi_hash_table_add_elem(&indirect_operations_fields_instrumented,
        operations_field_elem);
    spin_unlock_irqrestore(&indirect_operations_fields_instrumented_lock, flags);
    return 0;
}

void indirect_operations_fields_instrumented_remove(
    struct kedr_coi_hash_elem* operations_field_elem)
{
    unsigned long flags;
    
    //pr_info("Remove operations field %p from the table %p.",
    //    operations_field_elem->key, &indirect_operations_fields_instrumented);

    spin_lock_irqsave(&indirect_operations_fields_instrumented_lock, flags);
    kedr_coi_hash_table_remove_elem(&indirect_operations_fields_instrumented,
        operations_field_elem);
    spin_unlock_irqrestore(&indirect_operations_fields_instrumented_lock, flags);
}


//**Implementation of the global map for indirect operations***//
static struct kedr_coi_hash_table indirect_operations_instrumented;
DEFINE_SPINLOCK(indirect_operations_instrumented_lock);

int indirect_operations_instrumented_add_unique(
    struct kedr_coi_hash_elem* operations_elem)
{
    unsigned long flags;

    spin_lock_irqsave(&indirect_operations_instrumented_lock, flags);
    if(kedr_coi_hash_table_find_elem(&indirect_operations_instrumented,
        operations_elem->key))
    {
        spin_unlock_irqrestore(&indirect_operations_instrumented_lock, flags);
        return -EBUSY;
    }
    
    kedr_coi_hash_table_add_elem(&indirect_operations_instrumented,
        operations_elem);
    spin_unlock_irqrestore(&indirect_operations_instrumented_lock, flags);
    return 0;
}

void indirect_operations_instrumented_remove(
    struct kedr_coi_hash_elem* operations_elem)
{
    unsigned long flags;
    
    spin_lock_irqsave(&indirect_operations_instrumented_lock, flags);
    kedr_coi_hash_table_remove_elem(&indirect_operations_instrumented,
        operations_elem);
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
    
    result = kedr_coi_hash_table_init(&objects_directly_instrumented);
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
    kedr_coi_hash_table_destroy(&objects_directly_instrumented, NULL);
    kedr_coi_hash_table_destroy(&indirect_operations_instrumented, NULL);
    kedr_coi_hash_table_destroy(&indirect_operations_fields_instrumented, NULL);
}

//*********** API for create instrumentors ************************//
struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_create_indirect(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return instrumentor_indirect_create(
        operations_field_offset,
        operations_struct_size,
        replacements);
}

struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_create_direct(
    size_t object_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return instrumentor_direct_create(object_struct_size, replacements);
}


struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_create_indirect1(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return instrumentor_indirect1_create(
        operations_field_offset,
        operations_struct_size,
        replacements);
}

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return instrumentor_indirect_with_foreign_create(
        operations_field_offset,
        operations_struct_size,
        replacements);
}

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect1(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements)
{
    return instrumentor_indirect1_with_foreign_create(
        operations_field_offset,
        operations_struct_size,
        replacements);
}