#ifndef KEDR_COI_INSTRUMENTOR_INTERNAL_H
#define KEDR_COI_INSTRUMENTOR_INTERNAL_H

/*
 * Interface of KEDR ico instrumentation of the object with operations.
 */

#include <linux/module.h> /* 'struct module' definition */

/*
 * Define pair original operation -> replacement operation
 * Both operations should have same signature.
 */
struct kedr_coi_instrumentor_replacement
{
    size_t operation_offset;
    void* repl;
};

struct kedr_coi_instrumentor;
struct kedr_coi_instrumentor_operations
{
    int (*watch)(struct kedr_coi_instrumentor* instrumentor,
        void* object);
    
    int (*forget)(struct kedr_coi_instrumentor* instrumentor,
        void* object, int norestore);
    
    void* (*get_orig_operation)(struct kedr_coi_instrumentor* instrumentor,
        void* object,
        size_t operation_offset);

    int (*foreign_restore_copy)(struct kedr_coi_instrumentor* instrumentor,
        void* object,
        void* foreign_object);

    void (*destroy)(struct kedr_coi_instrumentor* instrumentor);
};

/* Instrumentor is an interface */
struct kedr_coi_instrumentor
{
    const struct kedr_coi_instrumentor_operations* ops;
};

static inline void
kedr_coi_instrumentor_init(struct kedr_coi_instrumentor *instrumentor,
    const struct kedr_coi_instrumentor_operations* ops)
{
    instrumentor->ops = ops;
}

/* Instrumentor's API*/
static inline int
kedr_coi_instrumentor_watch(
    struct kedr_coi_instrumentor* instrumentor,
    void* object)
{
    return instrumentor->ops->watch(instrumentor, object);
}

static inline int
kedr_coi_instrumentor_forget(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    int norestore)
{
    return instrumentor->ops->forget(instrumentor, object, norestore);
}

static inline void
kedr_coi_instrumentor_destroy(struct kedr_coi_instrumentor* instrumentor)
{
    instrumentor->ops->destroy(instrumentor);
}


static inline void*
kedr_coi_instrumentor_get_orig_operation(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    size_t operation_offset)
{
    BUG_ON(instrumentor->ops->get_orig_operation == NULL);

    return instrumentor->ops->get_orig_operation(instrumentor, object, operation_offset);
}

static inline int
kedr_coi_instrumentor_foreign_restore_copy(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    void* foreign_object)
{
    if(instrumentor->ops->foreign_restore_copy)
        return instrumentor->ops->foreign_restore_copy(instrumentor, object, foreign_object);
    else
        return -EPERM;
}

/* Constructors */
struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_indirect(
    size_t operations_field_offest,
    size_t operations_struct_size,
    struct kedr_coi_instrumentor_replacement* replace_pairs);

struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_direct(
    size_t object_struct_size,
    struct kedr_coi_instrumentor_replacement* replace_pairs);

struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_indirect_with_foreign(
    size_t operations_field_offest,
    size_t operations_struct_size,
    size_t foreign_operations_field_offset,
    struct kedr_coi_instrumentor_replacement* replace_pairs);



#endif /* KEDR_COI_INSTRUMENTOR_INTERNAL_H */