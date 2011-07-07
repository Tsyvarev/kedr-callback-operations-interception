#ifndef KEDR_COI_BASE_INTERNAL_H
#define KEDR_COI_BASE_INTERNAL_H

/*
 * Definition of common kedr_coi_interceptor structure.
 */

#include <linux/module.h>

#include <kedr-coi/operations_interception.h>

#include "kedr_coi_global_map_internal.h"

struct kedr_coi_interceptor_operations
{
    int (*start)(struct kedr_coi_interceptor* interceptor);
    void (*stop)(struct kedr_coi_interceptor* interceptor);
    
    int (*watch)(struct kedr_coi_interceptor* interceptor,
        void* object);
    
    int (*forget)(struct kedr_coi_interceptor* interceptor,
        void* object, int norestore);
    
    int (*payload_register)(struct kedr_coi_interceptor* interceptor,
        struct kedr_coi_payload* payload);

    void (*payload_unregister)(struct kedr_coi_interceptor* interceptor,
        struct kedr_coi_payload* payload);

    int (*payload_foreign_register)(struct kedr_coi_interceptor* interceptor,
        struct kedr_coi_payload_foreign* payload);

    void (*payload_foreign_unregister)(struct kedr_coi_interceptor* interceptor,
        struct kedr_coi_payload_foreign* payload);

    void* (*get_orig_operation)(struct kedr_coi_interceptor* interceptor,
        const void* object,
        size_t operation_offset);

    int (*foreign_restore_copy)(struct kedr_coi_interceptor* interceptor,
        void* object,
        void* foreign_object);

    void (*destroy)(struct kedr_coi_interceptor* interceptor);
};

/* Basically, interceptor is an virtual class*/
struct kedr_coi_interceptor
{
    const char* name;
    const struct kedr_coi_interceptor_operations* ops;
    // Other fields should be interpreted as private
    int state;
};


void
interceptor_common_init(
    struct kedr_coi_interceptor* interceptor,
    const char* name,
    const struct kedr_coi_interceptor_operations* ops);

#endif /* KEDR_COI_BASE_INTERNAL_H */