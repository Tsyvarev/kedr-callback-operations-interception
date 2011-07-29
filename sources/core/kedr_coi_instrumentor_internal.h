#ifndef KEDR_COI_INSTRUMENTOR_INTERNAL_H
#define KEDR_COI_INSTRUMENTOR_INTERNAL_H

/*
 * Interface of KEDR ico instrumentation of the object with operations.
 */

#include <linux/types.h> /* size_t */

struct kedr_coi_instrumentor;

/* Instrumentor API */

int kedr_coi_instrumentor_watch(
    struct kedr_coi_instrumentor* instrumentor,
    void* object);

int kedr_coi_instrumentor_forget(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    int norestore);

void kedr_coi_instrumentor_destroy(
    struct kedr_coi_instrumentor* instrumentor,
    void (*trace_unforgotten_object)(void* object));


/*
 * Return original operations of the object.
 * 
 * NOTE: If object doesn't registered but its operations are recognized
 * as replaced, fill data corresponded to these operations but
 * return 1.
 */
int
kedr_coi_instrumentor_get_orig_operations(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    //out param
    const void** ops_orig);


struct kedr_coi_replacement
{
    size_t operation_offset;
    void* repl;
};

struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_indirect(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements);

struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_direct(
    size_t object_struct_size,
    const struct kedr_coi_replacement* replacements);

/*
 * This function should be called before using any of the functions above.
 */
int kedr_coi_instrumentors_init(void);

/*
 * This function should be called when instrumentors are not needed.
 */
void kedr_coi_instrumentors_destroy(void);

#endif /* KEDR_COI_INSTRUMENTOR_INTERNAL_H */