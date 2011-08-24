#ifndef KEDR_COI_INSTRUMENTOR_INTERNAL_H
#define KEDR_COI_INSTRUMENTOR_INTERNAL_H

/*
 * Interface of KEDR ico instrumentation of the object with operations.
 */

#include <linux/types.h> /* size_t */
#include <linux/spinlock.h> /* spinlocks */

#include "kedr_coi_hash_table.h"

//***********Simple implementation of objects with interfaces ********//
// Basic interface structure
struct kedr_coi_interface
{
    // RTTI support may be here :)
};

// Basic structure of object which implements interface
struct kedr_coi_object
{
    struct kedr_coi_interface* interface;
};

//*************Common instrumentor structure*****************************
// Per-object data
struct kedr_coi_instrumentor_object_data
{
    // All fields - protected
    struct kedr_coi_hash_elem object_elem;
};

// Interface of the implementation of concrete instrumentor
struct kedr_coi_instrumentor_impl_iface
{
    struct kedr_coi_interface base;

    struct kedr_coi_instrumentor_object_data* (*alloc_object_data)(
        struct kedr_coi_object* iface_impl);

    void (*free_object_data)(
        struct kedr_coi_object* iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data);
    
    int (*replace_operations)(
        struct kedr_coi_object* iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data_new,
        void* object);
    
    void (*clean_replacement)(
        struct kedr_coi_object* iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data,
        void* object,
        int norestore);
    
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
        struct kedr_coi_object* iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data,
        void* object);
    
    /*
     * Called when instrumentor was destroyed.
     */
    void (*destroy_impl)(struct kedr_coi_object* iface_impl);
};

struct kedr_coi_instrumentor;
/* Instrumentor as virtual object */
struct kedr_coi_instrumentor_type
{
    // Virtual destructor
    void (*finalize)(struct kedr_coi_instrumentor* instrumentor);
    size_t struct_offset;
};

struct kedr_coi_instrumentor
{
    // All fields - protected
    struct kedr_coi_hash_table objects;
    spinlock_t lock;
    
    // used only while instrumentor is being deleted
    void (*trace_unforgotten_object)(void* object);
   
    struct kedr_coi_instrumentor_type* real_type;
    
    struct kedr_coi_object* i_iface_impl;
};

//************* Instrumentor API ********************//

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


//**********Instrumentor for objects with its own operations***********

// Derive from common interface
struct kedr_coi_instrumentor_normal_impl_iface
{
    struct kedr_coi_instrumentor_impl_iface i_iface;
    
    int (*get_orig_operation)(
        struct kedr_coi_object* iface_impl_normal,
        struct kedr_coi_instrumentor_object_data* object_data,
        const void* object,
        size_t operation_offset,
        void** op_orig);
    
    int (*get_orig_operation_nodata)(
        struct kedr_coi_object* iface_impl_normal,
        const void* object,
        size_t operation_offset,
        void** op_orig);

};

struct kedr_coi_instrumentor_normal
{
    // Public
    struct kedr_coi_instrumentor instrumentor_common;
    // Protected
    struct kedr_coi_object* in_iface_impl;
};

// For create instrumentors of concrete type
struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_normal_create(
    struct kedr_coi_object* in_iface_impl);

//***********  API for 'normal' instrumentor************************

/*
 * Return original operation of the object.
 * 
 * NOTE: If object doesn't registered but its operations are recognized
 * as replaced, fill data corresponded to these operations but
 * return 1.
 */
int
kedr_coi_instrumentor_get_orig_operation(
    struct kedr_coi_instrumentor_normal* instrumentor,
    const void* object,
    size_t operation_offset,
    void** op_orig);


//********** Creation of the normal instrumentors ********************

struct kedr_coi_replacement
{
    size_t operation_offset;
    void* repl;
};

struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_create_indirect(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements);

struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_create_direct(
    size_t object_struct_size,
    const struct kedr_coi_replacement* replacements);

//************** Instrumentor for foreign operations *****************/
struct kedr_coi_instrumentor_with_foreign;

struct kedr_coi_foreign_instrumentor_impl_iface
{
    struct kedr_coi_instrumentor_impl_iface i_iface;

    /*
     * Called when 'kedr_coi_bind_prototype_with_object' is called,
     * but neither prototype_object or normal object is watched.
     * 
     * Operation should restore object operations
     * and return original operation for object.
     * 
     * NOTE: 'Restore object operations' means that object operations
     * will work AS original ones, but they may be not equal to original ones.
     */

    int (*restore_foreign_operations_nodata)(
        struct kedr_coi_object* if_iface_impl,
        const void* prototype_object,
        void* object,
        size_t operation_offset,
        void** op_orig);

    /*
     * Called when 'kedr_coi_bind_prototype_with_object' is called,
     * for prototype_object watched, but allocation of the object_data
     * for object was failed.
     * 
     * Operation should restore object operations
     * and return original operation for object.
     * 
     * NOTE: 'Restore object operations' means that object operations
     * will work AS original ones, but they may be not equal to original ones.
     */

    int (*restore_foreign_operations)(
        struct kedr_coi_object* if_iface_impl,
        struct kedr_coi_instrumentor_object_data* prototype_data,
        const void* prototype_object,
        void* object,
        size_t operation_offset,
        void** op_orig);

};

struct kedr_coi_foreign_instrumentor
{
    struct kedr_coi_instrumentor instrumentor_common;
    
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded;

    struct kedr_coi_object* if_iface_impl;
};

struct kedr_coi_instrumentor_with_foreign_impl_iface
{
    struct kedr_coi_instrumentor_normal_impl_iface in_iface;
    // Factory method
    struct kedr_coi_object* (*create_foreign_indirect)(
        struct kedr_coi_object* iwf_iface_impl,
        size_t operations_field_offset,
        const struct kedr_coi_replacement* replacements);

    /*
     * Called when 'kedr_coi_bind_prototype_with_object' is called for
     * prototype_object which is watched.
     * 
     * 'object_data_new' are object data which are allocated for
     * normal instrumentor.
     * 
     * Operation should return replaced operation for newly watched object.
     */
    int (*replace_operations_from_prototype)(
        struct kedr_coi_object* iwf_iface_impl,
        struct kedr_coi_object* if_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data_new,
        void* object,
        struct kedr_coi_instrumentor_object_data* prototype_data,
        void* protorype_object,
        size_t operation_offset,
        void** op_repl);
    
    /*
     * Called when 'kedr_coi_bind_prototype_with_object' is called for
     * prototype_object which is watched when object itself is already watched.
     * 
     * 'object_data' are object data corresponded to the watched object.
     * 
     * Operation should return replaced operation for watched
     * object.
     */

    int (*update_operations_from_prototype)(
        struct kedr_coi_object* iwf_iface_impl,
        struct kedr_coi_object* if_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data,
        void* object,
        struct kedr_coi_instrumentor_object_data* prototype_data,
        void* protorype_object,
        size_t operation_offset,
        void** op_repl);
    
    /*
     * Called when 'kedr_coi_bind_prototype_with_object' is called for
     * prototype_object which is not watched when object itself is
     * already watched.
     * 
     * Prototype object may be not watched if:
     * 1) Operations for it was copied from another object
     * which is watched.
     * 2) It use same operations as watched object use.
     *
     * Normal object may be watched if:
     * 1) Object with same address was previousely watched but is
     * not forgotten when deleted.
     * 2) Operation interception mechanizm is so, that foreign
     * intermediate operation is called whenever object is already
     * watched or not.
     * 
     * 'object_data' are object data corresponded to the watched object.
     *
     * Functions should do whatever is required for correct behaviour of
     * kedr_coi_bind_prototype_with_object() and return chained operation.
     */

    int (*chain_operation)(
        struct kedr_coi_object* iwf_iface_impl,
        struct kedr_coi_object* if_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data,
        void* object,
        void* protorype_object,
        size_t operation_offset,
        void** op_chained);
};

struct kedr_coi_instrumentor_with_foreign
{
    struct kedr_coi_instrumentor_normal _instrumentor_normal;
    
    struct kedr_coi_object* iwf_iface_impl;
};

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create(
    struct kedr_coi_object* iwf_iface_impl);

//***********  API for 'foreign' instrumentor************************

int
kedr_coi_foreign_instrumentor_bind_prototype_with_object(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    void* prototype_object,
    void* object,
    size_t operation_offset,
    void** op_chained);


//*********** Foreign instrumentor creation ************************

struct kedr_coi_foreign_instrumentor*
kedr_coi_instrumentor_with_foreign_create_foreign_indirect(
    struct kedr_coi_instrumentor_with_foreign* instrumentor,
    size_t operations_field_offset,
    const struct kedr_coi_replacement* replacements);

/*
 * Create indirect instrumentor with foreign support
 */
struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect(
    size_t operations_field_offset,
    size_t operations_struct_size,
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