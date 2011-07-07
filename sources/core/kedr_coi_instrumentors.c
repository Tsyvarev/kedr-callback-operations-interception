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
#include "kedr_coi_global_map_internal.h"
#include "kedr_coi_hash_table_internal.h"

/* Instrumentor for indirect operations (most used)*/
struct instrumentor_indirect_object_data
{
    // Replaced operations of object
    char* ops;
    // Original operations of object
    const char* ops_orig;
    // lock for concurrent access on update to 'ops' field.
    spinlock_t lock;
};


struct kedr_coi_instrumentor_indirect
{
    struct kedr_coi_instrumentor base;
    /*
     * (object_key) => (object_data)
     */
    struct kedr_coi_hash_table* objects;
    
    size_t operations_field_offset;
    size_t operations_struct_size;

    struct kedr_coi_instrumentor_replacement* replacements;
};

#define instrumentor_indirect(instrumentor) container_of(instrumentor, struct kedr_coi_instrumentor_indirect, base)

static inline const void*
instrumentor_indirect_object_key(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const void* object)
{
    return (const char*)object + instrumentor->operations_field_offset;
}

static inline const void*
instrumentor_indirect_object_key_global(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const void* object)
{
    return instrumentor_indirect_object_key(instrumentor, object);
}

static inline const char**
instrumentor_indirect_object_ops(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    void* object)
{
    void* operations_offset =
        (char*) object + instrumentor->operations_field_offset;
    return (const char**)operations_offset;
}

static void
instrumentor_indirect_object_data_fill(
    struct instrumentor_indirect_object_data* object_data,
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const char* object_ops)
{
    BUG_ON(object_data == NULL);
    BUG_ON(object_ops == NULL);
    
    object_data->ops_orig = object_ops;
    memcpy(object_data->ops, object_ops,
        instrumentor->operations_struct_size);
    if(instrumentor->replacements)
    {
        struct kedr_coi_instrumentor_replacement* replacement;
        for(replacement = instrumentor->replacements;
            replacement->operation_offset != -1;
            replacement++)
            *((void**)(object_data->ops + replacement->operation_offset)) = replacement->repl;
    }
}

static struct instrumentor_indirect_object_data*
instrumentor_indirect_object_data_create(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const char* object_ops)
{
    // TODO: kmem_cache may be more appropriate
    struct instrumentor_indirect_object_data* object_data =
        kmalloc(sizeof(*object_data), GFP_KERNEL);
    if(object_data == NULL) return NULL;
    
    // TODO: kmem_cache may be more appropriate
    object_data->ops = kmalloc(instrumentor->operations_struct_size, GFP_KERNEL);
    if(object_data->ops == NULL)
    {
        kfree(object_data);
        return NULL;
    }
    
    spin_lock_init(&object_data->lock);
    
    instrumentor_indirect_object_data_fill(object_data,
        instrumentor,
        object_ops);
    
    return object_data;
}

static void
instrumentor_indirect_object_data_free(
    struct instrumentor_indirect_object_data* object_data)
{
    kfree(object_data->ops);
    kfree(object_data);
}



//object_data should be flushed at that moment(barrier)
static int
instrumentor_indirect_replace_operations(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const char** object_ops_p,
    struct instrumentor_indirect_object_data* object_data)
{
    *object_ops_p = object_data->ops;
    return 0;
}

static int
instrumentor_indirect_update_operations(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const char** object_ops_p,
    struct instrumentor_indirect_object_data* object_data)
{
    /* 
     * Whether need to update operations in the object, assuming 
     * object_data already updated.
     * 
     * We want to update this operations AFTER
     * object_data is flushed.
     * 
     * ~ volatile write
     * 
     * This DOES NOT provide 100% data coherence with calling of operations, 
     * because we cannot control this call at all.
     * But this may INCREASE this coherence, if operations call use some
     * synchronization mechanizm.
     */
    int need_update_operations = 1;

    unsigned long flags;
    // Only one dereference of object operations
    const char* object_ops = *object_ops_p;

    spin_lock_irqsave(&object_data->lock, flags);

    if(object_ops == object_data->ops)
    {
        // nothing to change
        need_update_operations = 0;
    }
    else if(object_ops == object_data->ops_orig)
    {
        // only need to update operations in the object
    }
    else
    {
        // recalculate replaced operations
        instrumentor_indirect_object_data_fill(object_data,
            instrumentor, object_ops);
    }
    
    spin_unlock_irqrestore(&object_data->lock, flags);
    
    if(need_update_operations)
    {
        //spin_unlock is a write memory barrier, so we needn't another one
        *object_ops_p = object_data->ops;
    }
    
    return 1;// '1' is a signal about 'update', not an error
}


static int
instrumentor_indirect_ops_watch(
    struct kedr_coi_instrumentor* instrumentor,
    void* object)
{
    const void* key;
    struct instrumentor_indirect_object_data* object_data_new;
    struct instrumentor_indirect_object_data* object_data;
    const char** object_ops_p;
    
    struct kedr_coi_instrumentor_indirect* instrumentor_real =
        instrumentor_indirect(instrumentor);

    key = instrumentor_indirect_object_key(instrumentor_real, object);
    object_ops_p = instrumentor_indirect_object_ops(instrumentor_real, object);
    
    object_data_new = instrumentor_indirect_object_data_create(
        instrumentor_real, *object_ops_p);
    if(object_data_new == NULL)
    {
        return -ENOMEM;
    }

    object_data = kedr_coi_hash_table_add(instrumentor_real->objects,
        key, object_data_new);
    
    if(IS_ERR(object_data) < 0)
    {
        // Error
        instrumentor_indirect_object_data_free(object_data_new);

        return PTR_ERR(object_data);
    }
    else if(object_data != NULL)
    {
        // Key already registered
        instrumentor_indirect_object_data_free(object_data_new);
        
        return instrumentor_indirect_update_operations(
            instrumentor_real,
            object_ops_p,
            object_data);
    }
    else
    {
        // New data was successfully added
        // Add key to the global map
        int result;
        const void* global_key = instrumentor_indirect_object_key_global(
            instrumentor_real,
            object);

        result = kedr_coi_global_map_add_key(global_key);
        
        if(result < 0)
        {
            // Rollback because of error.
            void* object_data_tmp = kedr_coi_hash_table_remove(instrumentor_real->objects, key);
            BUG_ON(object_data_tmp != object_data_new);

            instrumentor_indirect_object_data_free(object_data_new);
            if(result == -EBUSY)
            {
                pr_err("Object are already instrumented by another instrumentor.");
            }
            return result;
        }

        return instrumentor_indirect_replace_operations(
            instrumentor_real,
            object_ops_p,
            object_data_new);
    }
}

static int
instrumentor_indirect_ops_forget(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    int norestore)
{
    const void* key;
    struct instrumentor_indirect_object_data* object_data;
    const char** object_ops_p;
    
    struct kedr_coi_instrumentor_indirect* instrumentor_real =
        instrumentor_indirect(instrumentor);

    key = instrumentor_indirect_object_key(instrumentor_real, object);
    object_ops_p = instrumentor_indirect_object_ops(instrumentor_real, object);
    
    object_data = kedr_coi_hash_table_remove(instrumentor_real->objects, key);

    if(IS_ERR(object_data))
    {
        int error = PTR_ERR(object_data);
        if(error == -ENOENT)
        {
            /*
             *  Object wasn't watched.
             * Not an error because wacth_for_object may has failed before.
             */
            return 1;
        }
        
        return 1;// some other error
    }
    
    if(!norestore)
    {
        // Without lock because object is deleted, so no one should access to it.
        if(*object_ops_p == object_data->ops)
            *object_ops_p = object_data->ops_orig;
    }
    instrumentor_indirect_object_data_free(object_data);

    {
        const void* global_key = instrumentor_indirect_object_key_global(
            instrumentor_real,
            object);
        
        kedr_coi_global_map_delete_key(global_key);

    }

    return 0;
}


static void*
instrumentor_indirect_ops_get_orig_operation(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    size_t operation_offset)
{
    void* orig_operation;

    const void* key;
    struct instrumentor_indirect_object_data* object_data;
    
    unsigned long flags;

    struct kedr_coi_instrumentor_indirect* instrumentor_real =
        instrumentor_indirect(instrumentor);

    key = instrumentor_indirect_object_key(instrumentor_real, object);
    
    object_data = kedr_coi_hash_table_get(instrumentor_real->objects, key);
    
    BUG_ON(IS_ERR(object_data));

    spin_lock_irqsave(&object_data->lock, flags);
    
    orig_operation = *((void* const*)(object_data->ops_orig + operation_offset));
    
    spin_unlock_irqrestore(&object_data->lock, flags);
    
    return orig_operation;
}

static void instrumentor_indirect_table_free_data(void* data, const void* key,
    void* user_data)
{
    instrumentor_indirect_object_data_free(data);
}


/* For foreign instrumentor*/
static void
kedr_coi_instrumentor_indirect_clean(
    struct kedr_coi_instrumentor_indirect* instrumentor)
{
    kedr_coi_hash_table_destroy(instrumentor->objects,
        instrumentor_indirect_table_free_data, NULL);
}


static void
instrumentor_indirect_ops_destroy(struct kedr_coi_instrumentor* instrumentor)
{
    struct kedr_coi_instrumentor_indirect* instrumentor_real =
        instrumentor_indirect(instrumentor);

    kedr_coi_instrumentor_indirect_clean(instrumentor_real);
    
    kfree(instrumentor_real);
}


static struct kedr_coi_instrumentor_operations instrumentor_indirect_ops =
{
    .watch = instrumentor_indirect_ops_watch,
    .forget = instrumentor_indirect_ops_forget,
    
    .get_orig_operation = instrumentor_indirect_ops_get_orig_operation,
    
    .destroy = instrumentor_indirect_ops_destroy,
};

/* For reuse in foreign instrumentor. */
static int
kedr_coi_instrumentor_indirect_init(
    struct kedr_coi_instrumentor_indirect* instrumentor,
    const struct kedr_coi_instrumentor_operations* ops,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_instrumentor_replacement* replacements)
{
    kedr_coi_instrumentor_init(&instrumentor->base, ops);

    instrumentor->objects = kedr_coi_hash_table_create();
    if(instrumentor->objects == NULL)
    {
        return -ENOMEM;
    }

    instrumentor->operations_field_offset = operations_field_offset;
    instrumentor->operations_struct_size = operations_struct_size;
    instrumentor->replacements = replacements;
    
    return 0;
}

struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_indirect(
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_instrumentor_replacement* replacements)
{
    struct kedr_coi_instrumentor_indirect* instrumentor;
    
    instrumentor = kmalloc(sizeof(*instrumentor), GFP_KERNEL);
    
    if(instrumentor == NULL)
    {
        pr_err("Failed to allocate instrumentor.");

        return NULL;
    }
    
    if(kedr_coi_instrumentor_indirect_init(instrumentor,
            &instrumentor_indirect_ops,
            operations_field_offset,
            operations_struct_size,
            replacements))
    {
        kfree(instrumentor);
        return NULL;
    }

    return &instrumentor->base;
}

/* Instrumentor for direct operations (rarely used)*/
struct instrumentor_direct_object_data
{
    // Object with filled original operations(other fields are uninitialized)
    char* object_ops_orig;
};


struct kedr_coi_instrumentor_direct
{
    struct kedr_coi_instrumentor base;
    /*
     * (object_key) => (object_data)
     */
    struct kedr_coi_hash_table* objects;
    
    size_t object_struct_size;

    struct kedr_coi_instrumentor_replacement* replacements;
};

#define instrumentor_direct(instrumentor) container_of(instrumentor, struct kedr_coi_instrumentor_direct, base)

static inline const void*
instrumentor_direct_object_key(
    struct kedr_coi_instrumentor_direct* instrumentor,
    const void* object)
{
    return object;
}

static inline const void*
instrumentor_direct_object_key_global(
    struct kedr_coi_instrumentor_direct* instrumentor,
    const void* object)
{
    return instrumentor_direct_object_key(instrumentor, object);
}

static struct instrumentor_direct_object_data*
instrumentor_direct_object_data_create(
    struct kedr_coi_instrumentor_direct* instrumentor,
    const void* object)
{
    // TODO: kmem_cache may be more appropriate
    struct instrumentor_direct_object_data* object_data =
        kmalloc(sizeof(*object_data), GFP_KERNEL);
    if(object_data == NULL) return NULL;
    
    // TODO: kmem_cache may be more appropriate
    object_data->object_ops_orig = kmalloc(instrumentor->object_struct_size, GFP_KERNEL);
    if(object_data->object_ops_orig == NULL)
    {
        kfree(object_data);
        return NULL;
    }
    
    memcpy(object_data->object_ops_orig, object, instrumentor->object_struct_size);
    
    return object_data;
}

static void
instrumentor_direct_object_data_free(
    struct instrumentor_direct_object_data* object_data)
{
    kfree(object_data->object_ops_orig);
    kfree(object_data);
}

static int
instrumentor_direct_ops_watch(
    struct kedr_coi_instrumentor* instrumentor,
    void* object)
{
    const void* key;
    struct instrumentor_direct_object_data* object_data_new;
    struct instrumentor_direct_object_data* object_data;
    
    struct kedr_coi_instrumentor_direct* instrumentor_real =
        instrumentor_direct(instrumentor);

    key = instrumentor_direct_object_key(instrumentor_real, object);
    
    object_data_new = instrumentor_direct_object_data_create(
        instrumentor_real, object);
    if(object_data_new == NULL)
    {
        return -ENOMEM;
    }

    object_data = kedr_coi_hash_table_add(instrumentor_real->objects,
        key, object_data_new);
    
    if(IS_ERR(object_data) < 0)
    {
        // Error
        instrumentor_direct_object_data_free(object_data_new);

        return PTR_ERR(object_data);
    }
    else if(object_data != NULL)
    {
        // Key already registered
        instrumentor_direct_object_data_free(object_data_new);
        
        return -EEXIST;
    }

    // New data was successfully added
    // Add key to the global map
    {
        int result;
        const void* global_key = instrumentor_direct_object_key_global(
            instrumentor_real,
            object);

        result = kedr_coi_global_map_add_key(global_key);
        
        if(result < 0)
        {
            // Rollback because of error.
            void* object_data_tmp = kedr_coi_hash_table_remove(instrumentor_real->objects, key);
            BUG_ON(object_data_tmp != object_data_new);

            instrumentor_direct_object_data_free(object_data_new);
            if(result == -EBUSY)
            {
                pr_err("Object is already instrumented by another instrumentor.");
            }
            return result;
        }
    }
    
    if(instrumentor_real->replacements)
    {
        struct kedr_coi_instrumentor_replacement* replacement;
        for(replacement = instrumentor_real->replacements;
            replacement->operation_offset != -1;
            replacement++)
            {
                void* operation_addr = (char*)object + replacement->operation_offset;
                *((void**)operation_addr) = replacement->repl;
            }
    }

    return 0;
}

static int
instrumentor_direct_ops_forget(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    int norestore)
{
    const void* key;
    struct instrumentor_direct_object_data* object_data;
    
    struct kedr_coi_instrumentor_direct* instrumentor_real =
        instrumentor_direct(instrumentor);

    key = instrumentor_direct_object_key(instrumentor_real, object);
    
    object_data = kedr_coi_hash_table_remove(instrumentor_real->objects, key);

    if(IS_ERR(object_data))
    {
        int error = PTR_ERR(object_data);
        if(error == -ENOENT)
        {
            /*
             *  Object wasn't watched.
             * Not an error because wacth_for_object may has failed before.
             */
            return 1;
        }
        
        return 1;// some other error
    }
    
    if(!norestore && instrumentor_real->replacements)
    {
        struct kedr_coi_instrumentor_replacement* replacement;
        
        for(replacement = instrumentor_real->replacements;
            replacement->operation_offset != -1;
            replacement++)
        {
            void* operation_addr = (char*)object + replacement->operation_offset;
            void* operation_orig_addr =
                object_data->object_ops_orig + replacement->operation_offset;

            *((void**)operation_addr) = *((void**)operation_orig_addr);
        }
    }


    instrumentor_direct_object_data_free(object_data);

    {
        const void* global_key = instrumentor_direct_object_key_global(
            instrumentor_real,
            object);
        
        kedr_coi_global_map_delete_key(global_key);

    }

    return 0;
}

static void*
instrumentor_direct_ops_get_orig_operation(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    size_t operation_offset)
{
    const void* key;
    struct instrumentor_direct_object_data* object_data;
    
    struct kedr_coi_instrumentor_direct* instrumentor_real =
        instrumentor_direct(instrumentor);

    key = instrumentor_direct_object_key(instrumentor_real, object);
    
    object_data = kedr_coi_hash_table_get(instrumentor_real->objects, key);
    
    BUG_ON(IS_ERR(object_data));
    
    return *(void**)(object_data->object_ops_orig + operation_offset);
}

static void instrumentor_direct_table_free_data(void* data, const void* key,
    void* user_data)
{
    instrumentor_direct_object_data_free(data);
}

static void
instrumentor_direct_ops_destroy(struct kedr_coi_instrumentor* instrumentor)
{
    struct kedr_coi_instrumentor_direct* instrumentor_real =
        instrumentor_direct(instrumentor);

    kedr_coi_hash_table_destroy(instrumentor_real->objects,
        instrumentor_direct_table_free_data, NULL);
    
    kfree(instrumentor_real);
}


static struct kedr_coi_instrumentor_operations instrumentor_direct_ops =
{
    .watch = instrumentor_direct_ops_watch,
    .forget = instrumentor_direct_ops_forget,
    
    .get_orig_operation = instrumentor_direct_ops_get_orig_operation,
    
    .destroy = instrumentor_direct_ops_destroy,
};

struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_direct(
    size_t object_struct_size,
    struct kedr_coi_instrumentor_replacement* replacements)
{
    struct kedr_coi_instrumentor_direct* instrumentor;
    
    instrumentor = kmalloc(sizeof(*instrumentor), GFP_KERNEL);
    
    if(instrumentor == NULL)
    {
        pr_err("Failed to allocate instrumentor.");

        return NULL;
    }
    
    kedr_coi_instrumentor_init(&instrumentor->base, &instrumentor_direct_ops);
    
    instrumentor->objects = kedr_coi_hash_table_create();
    if(instrumentor->objects == NULL)
    {
        kfree(instrumentor);
        
        return NULL;
    }

    instrumentor->object_struct_size = object_struct_size;
    instrumentor->replacements = replacements;
    
    return &instrumentor->base;
}

/* 
 * Instrumentor for foreign operations.
 * 
 * It is very similar to instrumentor for indirect operations.
 */

struct kedr_coi_instrumentor_foreign
{
    struct kedr_coi_instrumentor_indirect indirect;
    
    size_t foreign_operations_field_offset;
};

#define instrumentor_foreign(instrumentor) container_of(instrumentor_indirect(instrumentor), struct kedr_coi_instrumentor_foreign, indirect)

static int instrumentor_foreign_ops_foreign_restore_copy(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    void* foreign_object)
{
    const char* orig_operations;
    const char** foreign_operations_p;

    const void* key;
    struct instrumentor_indirect_object_data* object_data;
    
    unsigned long flags;

    struct kedr_coi_instrumentor_foreign* instrumentor_real =
        instrumentor_foreign(instrumentor);

    key = instrumentor_indirect_object_key(&instrumentor_real->indirect, object);
    
    object_data = kedr_coi_hash_table_get(instrumentor_real->indirect.objects, key);
    
    BUG_ON(IS_ERR(object_data));

    spin_lock_irqsave(&object_data->lock, flags);
    
    orig_operations = object_data->ops_orig;
    
    spin_unlock_irqrestore(&object_data->lock, flags);
    
    foreign_operations_p = (const char**)((char*)foreign_object +
        instrumentor_real->foreign_operations_field_offset);
    
    *foreign_operations_p = orig_operations;
    
    return 0;
   
}

static void
instrumentor_foreign_ops_destroy(struct kedr_coi_instrumentor* instrumentor)
{
    struct kedr_coi_instrumentor_foreign* instrumentor_real =
        instrumentor_foreign(instrumentor);
    
    kedr_coi_instrumentor_indirect_clean(&instrumentor_real->indirect);
    
    kfree(instrumentor_real);
}

static struct kedr_coi_instrumentor_operations instrumentor_foreign_ops =
{
    .watch = instrumentor_indirect_ops_watch,
    .forget = instrumentor_indirect_ops_forget,
     
    .foreign_restore_copy = instrumentor_foreign_ops_foreign_restore_copy,
    
    .destroy = instrumentor_foreign_ops_destroy,
};


struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_indirect_with_foreign(
    size_t operations_field_offset,
    size_t operations_struct_size,
    size_t foreign_operations_field_offset,
    struct kedr_coi_instrumentor_replacement* replacements)
{
    struct kedr_coi_instrumentor_foreign* instrumentor =
        kmalloc(sizeof(*instrumentor), GFP_KERNEL);
    
    if(instrumentor == NULL)
    {
        pr_err("Failed to allocate instrumentor");
        return NULL;
    }
    
    if(kedr_coi_instrumentor_indirect_init(&instrumentor->indirect,
        &instrumentor_foreign_ops,
        operations_field_offset,
        operations_struct_size,
        replacements))
    {
        kfree(instrumentor);
        
        return NULL;
    }
    
    instrumentor->foreign_operations_field_offset = foreign_operations_field_offset;
    
    return &instrumentor->indirect.base;
}