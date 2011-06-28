/*
 * Implementation of interceptors of different types.
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

#include <kedr-coi/operations_interception.h>

#include "kedr_coi_base_internal.h"
#include "kedr_coi_instrumentor_internal.h"

#include "kedr_coi_payloads_internal.h"
#include "kedr_coi_payloads_foreign_internal.h"


/* Interceptor for objects with its own operations. */

struct kedr_coi_interceptor_normal
{
    struct kedr_coi_interceptor base;
    
    struct kedr_coi_payload_container* payload_container;
    struct kedr_coi_instrumentor *instrumentor;
};

#define interceptor_normal(interceptor) container_of(interceptor, struct kedr_coi_interceptor_normal, base)

/* For implementation of children's start() */
static struct kedr_coi_instrumentor_replacement*
interceptor_normal_fix_payloads(
    struct kedr_coi_interceptor_normal* interceptor)
{
    return kedr_coi_payload_container_fix_payloads(interceptor->payload_container);
}

static void
interceptor_normal_release_payloads(
    struct kedr_coi_interceptor_normal* interceptor)
{
    kedr_coi_payload_container_release_payloads(interceptor->payload_container);
}


static void
interceptor_normal_stop(
    struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_interceptor_normal* interceptor_real =
        interceptor_normal(interceptor);
    
    kedr_coi_instrumentor_destroy(interceptor_real->instrumentor);
    
    interceptor_normal_release_payloads(interceptor_real);
}

static int
interceptor_normal_watch(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
    struct kedr_coi_interceptor_normal* interceptor_real =
        interceptor_normal(interceptor);
    
    return kedr_coi_instrumentor_watch(
        interceptor_real->instrumentor,
        object);
}

static int
interceptor_normal_forget(
    struct kedr_coi_interceptor* interceptor,
    void* object,
    int norestore)
{
    struct kedr_coi_interceptor_normal* interceptor_real =
        interceptor_normal(interceptor);
    
    return kedr_coi_instrumentor_forget(
        interceptor_real->instrumentor,
        object,
        norestore);
}

static void*
interceptor_normal_get_orig_operation(
    struct kedr_coi_interceptor* interceptor,
    void* object,
    size_t operation_offset)
{
    struct kedr_coi_interceptor_normal* interceptor_real =
        interceptor_normal(interceptor);
    
    return kedr_coi_instrumentor_get_orig_operation(
        interceptor_real->instrumentor,
        object,
        operation_offset);
}

static int interceptor_normal_payload_register(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload* payload)
{
    struct kedr_coi_interceptor_normal* interceptor_real =
        interceptor_normal(interceptor);
    
    return kedr_coi_payload_container_register_payload(
        interceptor_real->payload_container, payload);
}

static void interceptor_normal_payload_unregister(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload* payload)
{
    struct kedr_coi_interceptor_normal* interceptor_real =
        interceptor_normal(interceptor);
    
    kedr_coi_payload_container_unregister_payload(
        interceptor_real->payload_container, payload);
}


/* For implementation of children's constructors ('create' method) */
static int
interceptor_normal_init(struct kedr_coi_interceptor_normal* interceptor,
    const char* name,
    const struct kedr_coi_interceptor_operations* ops,
    struct kedr_coi_intermediate* intermediate_operations)
{
    interceptor_common_init(&interceptor->base,
        name,
        ops);
    
    interceptor->payload_container = kedr_coi_payload_container_create(
        intermediate_operations);

    return interceptor->payload_container ? 0 : -EINVAL;
}

static void
interceptor_normal_destroy(struct kedr_coi_interceptor_normal* interceptor)
{
    kedr_coi_payload_container_destroy(interceptor->payload_container,
        interceptor->base.name);
}

/* Interceptor for objects with indirect operations(mostly used). */

struct kedr_coi_interceptor_indirect
{
    struct kedr_coi_interceptor_normal normal;
    
    size_t operations_field_offset;
    size_t operations_struct_size;
};

#define interceptor_indirect(interceptor) container_of(interceptor_normal(interceptor), struct kedr_coi_interceptor_indirect, normal)

static int
interceptor_indirect_start(
    struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_instrumentor* instrumentor;
    struct kedr_coi_instrumentor_replacement* replacements;
    struct kedr_coi_interceptor_indirect* interceptor_real =
        interceptor_indirect(interceptor);
    
    replacements = interceptor_normal_fix_payloads(
        &interceptor_real->normal);
    
    if(IS_ERR(replacements))
        return PTR_ERR(replacements);
    
    instrumentor = kedr_coi_instrumentor_create_indirect(
        interceptor_real->operations_field_offset,
        interceptor_real->operations_struct_size,
        replacements);

    if(instrumentor == NULL)
    {
        interceptor_normal_release_payloads(&interceptor_real->normal);
        
        return -EINVAL;//some error
    }

    interceptor_real->normal.instrumentor = instrumentor;
    
    return 0;
}

static void
interceptor_indirect_destroy(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_interceptor_indirect* interceptor_real =
        interceptor_indirect(interceptor);
    
    interceptor_normal_destroy(&interceptor_real->normal);
    
    kfree(interceptor_real);
}


static struct kedr_coi_interceptor_operations interceptor_indirect_ops =
{
    .start = interceptor_indirect_start,
    .stop = interceptor_normal_stop,

    .watch = interceptor_normal_watch,
    .forget = interceptor_normal_forget,
    
    .get_orig_operation = interceptor_normal_get_orig_operation,
    
    .payload_register = interceptor_normal_payload_register,
    .payload_unregister = interceptor_normal_payload_unregister,
    
    .destroy = interceptor_indirect_destroy,
};

struct kedr_coi_interceptor*
kedr_coi_interceptor_create(const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_intermediate* intermediate_operations)
{
    int result;
    struct kedr_coi_interceptor_indirect* interceptor;
    
    interceptor = kzalloc(sizeof(*interceptor), GFP_KERNEL);
    
    if(interceptor == NULL)
    {
        pr_err("Cannot allocate operations interceptor.");
        return NULL;
    }
    
    result = interceptor_normal_init(&interceptor->normal,
        name,
        &interceptor_indirect_ops,
        intermediate_operations);
    
    if(result)
    {
        pr_err("Failed to initialize interceptor.");
        kfree(interceptor);
        
        return NULL;
    }

    interceptor->operations_field_offset = operations_field_offset;
    interceptor->operations_struct_size = operations_struct_size;
    
    return &interceptor->normal.base;
}


/* Interceptor for objects with direct operations. */

struct kedr_coi_interceptor_direct
{
    struct kedr_coi_interceptor_normal normal;
    
    size_t object_struct_size;
};

#define interceptor_direct(interceptor) container_of(interceptor_normal(interceptor), struct kedr_coi_interceptor_direct, normal)

static int
interceptor_direct_start(
    struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_instrumentor* instrumentor;
    struct kedr_coi_instrumentor_replacement* replacements;
    struct kedr_coi_interceptor_direct* interceptor_real =
        interceptor_direct(interceptor);
    
    replacements = interceptor_normal_fix_payloads(
        &interceptor_real->normal);
    
    if(IS_ERR(replacements))
        return PTR_ERR(replacements);
    
    instrumentor = kedr_coi_instrumentor_create_direct(
        interceptor_real->object_struct_size,
        replacements);

    if(instrumentor == NULL)
    {
        interceptor_normal_release_payloads(&interceptor_real->normal);
        
        return -EINVAL;//some error
    }

    interceptor_real->normal.instrumentor = instrumentor;
    
    return 0;
}

static void
interceptor_direct_destroy(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_interceptor_direct* interceptor_real =
        interceptor_direct(interceptor);
    
    interceptor_normal_destroy(&interceptor_real->normal);
    
    kfree(interceptor_real);
}


static struct kedr_coi_interceptor_operations interceptor_direct_ops =
{
    .start = interceptor_direct_start,
    .stop = interceptor_normal_stop,

    .watch = interceptor_normal_watch,
    .forget = interceptor_normal_forget,

    .get_orig_operation = interceptor_normal_get_orig_operation,

    .payload_register = interceptor_normal_payload_register,
    .payload_unregister = interceptor_normal_payload_unregister,
    
    .destroy = interceptor_direct_destroy,
};

struct kedr_coi_interceptor*
kedr_coi_interceptor_create_direct(const char* name,
    size_t object_struct_size,
    struct kedr_coi_intermediate* intermediate_operations)
{
    int result;
    struct kedr_coi_interceptor_direct* interceptor;
    
    interceptor = kzalloc(sizeof(*interceptor), GFP_KERNEL);
    
    if(interceptor == NULL)
    {
        pr_err("Cannot allocate operations interceptor.");
        return NULL;
    }
    
    result = interceptor_normal_init(&interceptor->normal,
        name,
        &interceptor_direct_ops,
        intermediate_operations);
    
    if(result)
    {
        pr_err("Failed to initialize interceptor.");
        kfree(interceptor);
        
        return NULL;
    }
    
    return &interceptor->normal.base;
}

/* Interceptor for objects with foreign operations */
struct kedr_coi_interceptor_foreign
{
    struct kedr_coi_interceptor base;
    
    struct kedr_coi_payload_foreign_container* payload_container;
    struct kedr_coi_instrumentor* instrumentor;
    
    size_t operations_field_offset;
    size_t operations_struct_size;
    
    size_t foreign_operations_field_offset;
};

#define interceptor_foreign(interceptor) container_of(interceptor, struct kedr_coi_interceptor_foreign, base)

static int
interceptor_foreign_start(
    struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_instrumentor* instrumentor;
    struct kedr_coi_instrumentor_replacement* replacements;
    struct kedr_coi_interceptor_foreign* interceptor_real =
        interceptor_foreign(interceptor);
    
    replacements = kedr_coi_payload_foreign_container_fix_payloads(
        interceptor_real->payload_container);
    
    if(IS_ERR(replacements))
        return PTR_ERR(replacements);
    
    instrumentor = kedr_coi_instrumentor_create_indirect_with_foreign(
        interceptor_real->operations_field_offset,
        interceptor_real->operations_struct_size,
        interceptor_real->foreign_operations_field_offset,
        replacements);

    if(instrumentor == NULL)
    {
        kedr_coi_payload_foreign_container_release_payloads(
            interceptor_real->payload_container);
        
        return -EINVAL;//some error
    }

    interceptor_real->instrumentor = instrumentor;
    
    return 0;
}


static void
interceptor_foreign_stop(
    struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_interceptor_foreign* interceptor_real =
        interceptor_foreign(interceptor);
    
    kedr_coi_instrumentor_destroy(interceptor_real->instrumentor);
    
    kedr_coi_payload_foreign_container_release_payloads(
        interceptor_real->payload_container);
}

static int
interceptor_foreign_watch(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
    struct kedr_coi_interceptor_foreign* interceptor_real =
        interceptor_foreign(interceptor);
    
    return kedr_coi_instrumentor_watch(
        interceptor_real->instrumentor,
        object);
}

static int
interceptor_foreign_forget(
    struct kedr_coi_interceptor* interceptor,
    void* object,
    int norestore)
{
    struct kedr_coi_interceptor_foreign* interceptor_real =
        interceptor_foreign(interceptor);
    
    return kedr_coi_instrumentor_forget(
        interceptor_real->instrumentor,
        object,
        norestore);
}

static int
interceptor_foreign_restore_copy(
    struct kedr_coi_interceptor* interceptor,
    void* object,
    void* foreign_object)
{
    struct kedr_coi_interceptor_foreign* interceptor_real =
        interceptor_foreign(interceptor);
    
    return kedr_coi_instrumentor_foreign_restore_copy(
        interceptor_real->instrumentor,
        object,
        foreign_object);
}

static int interceptor_foreign_payload_register(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload_foreign* payload)
{
    struct kedr_coi_interceptor_foreign* interceptor_real =
        interceptor_foreign(interceptor);
    
    return kedr_coi_payload_foreign_container_register_payload(
        interceptor_real->payload_container, payload);
}

static void interceptor_foreign_payload_unregister(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload_foreign* payload)
{
    struct kedr_coi_interceptor_foreign* interceptor_real =
        interceptor_foreign(interceptor);
    
    kedr_coi_payload_foreign_container_unregister_payload(
        interceptor_real->payload_container, payload);
}

static void
interceptor_foreign_destroy(struct kedr_coi_interceptor* interceptor)
{
    struct kedr_coi_interceptor_foreign* interceptor_real =
        interceptor_foreign(interceptor);
    
    kedr_coi_payload_foreign_container_destroy(interceptor_real->payload_container,
        interceptor->name);
    
    kfree(interceptor_real);
}

static struct kedr_coi_interceptor_operations interceptor_foreign_ops =
{
    .start = interceptor_foreign_start,
    .stop = interceptor_foreign_stop,

    .watch = interceptor_foreign_watch,
    .forget = interceptor_foreign_forget,
    
    .foreign_restore_copy = interceptor_foreign_restore_copy,
    
    .payload_foreign_register = interceptor_foreign_payload_register,
    .payload_foreign_unregister = interceptor_foreign_payload_unregister,
    
    .destroy = interceptor_foreign_destroy,
};

struct kedr_coi_interceptor*
kedr_coi_interceptor_create_foreign(
    const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    size_t foreign_operations_field_offset,
    struct kedr_coi_intermediate_foreign* intermediate_operations,
    struct kedr_coi_intermediate_foreign_info* intermediate_info)
{
    struct kedr_coi_interceptor_foreign* interceptor;
    
    interceptor = kmalloc(sizeof(*interceptor), GFP_KERNEL);
    
    if(interceptor == NULL)
    {
        pr_err("Cannot allocate operations interceptor.");
        
        return NULL;
    }
    
    interceptor->payload_container = kedr_coi_payload_foreign_container_create(
        intermediate_operations,
        intermediate_info);
    
    if(interceptor->payload_container == NULL)
    {
        kfree(interceptor);
        
        return NULL;
    }

    interceptor_common_init(&interceptor->base,
        name,
        &interceptor_foreign_ops);


    interceptor->operations_field_offset = operations_field_offset;
    interceptor->operations_struct_size = operations_struct_size;
    interceptor->foreign_operations_field_offset = foreign_operations_field_offset;

    return &interceptor->base;
}
