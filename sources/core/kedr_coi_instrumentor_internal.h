#ifndef KEDR_COI_INSTRUMENTOR_INTERNAL_H
#define KEDR_COI_INSTRUMENTOR_INTERNAL_H

/*
 * Interface of KEDR ico instrumentation of the object with operations.
 */

#include <linux/types.h> /* size_t */
#include <linux/spinlock.h> /* spinlocks */

#include "kedr_coi_hash_table.h"

/*
 * Description of one replacement for instrumentor.
 */
enum replacement_mode
{
    replace_null,
    replace_not_null,
    replace_all
};

struct kedr_coi_replacement
{
    size_t operation_offset;
    void* repl;
    int mode;
};

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

    /*
     * Called when 'watch' is called for a new object.
     * 
     * Should return object_data which will be set for that object,
     * or NULL on error. May sleep.
     */
    struct kedr_coi_instrumentor_object_data* (*alloc_object_data)(
        struct kedr_coi_object* i_iface_impl);

    /*
     * Called when 'watch' is called for a new object.
     * 
     * 'object_data_new' are data allocated with 'alloc_object_data'.
     * 
     * Executed in atomic context.
     * 
     * Should replace operations and fill object_data_new with data
     * described this replacement.
     * 
     * No other callback function which works with the same object may
     * be called concurrently with this. 'object_data_new' become
     * available for other functions only when this callback returns
     * successfully.
     */
    int (*replace_operations)(
        struct kedr_coi_object* i_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data_new,
        void* object);
    
    /*
     * Called when 'forget' is called for an object, which is watched.
     * 
     * 'object_data' are data concerned with this object.
     * 
     * Executed in atomic context.
     * 
     * Should remove from object data information about replacement and,
     * if norestore is 0, revert object's operations to their initial
     * state.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this. 'object_data' become unavailable
     * for other functions after this callback returns.
     */
    void (*clean_replacement)(
        struct kedr_coi_object* i_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data,
        void* object,
        int norestore);
    
    /*
     * Called after replace_operations() was failed, after
     * clean_replacement() is called or after update_operations()
     * returns 1.
     * 
     * Should free data previously allocated with alloc_object_data().
     */
    void (*free_object_data)(
        struct kedr_coi_object* i_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data);


    /*
     *  Called when 'watch' is called for object for which we already
     * watch for.
     * 
     *  Should update object_data and operations in the object for
     * continue watch for an object.
     * 
     * If need to recreate object_data, should restore operations
     * in the object and return 1. After that, replace_operations()
     * will be called for newly created object data.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this. 'object_data' become
     * unavailable for other functions after this callback returns 1.
     */
    int (*update_operations)(
        struct kedr_coi_object* i_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data,
        void* object);
    
    /*
     * Called when instrumentor was destroyed.
     */
    void (*destroy_impl)(struct kedr_coi_object* i_iface_impl);
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
    
    /*
     * Called when 'get_orig_operation' is called for an object which
     * is watched.
     * 
     * 'object_data' are data corresponded to this object.
     * 
     * Executed in atomic context.
     * 
     * Should return original object's operation at the given offset or
     * ERR_PTR() on error.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this.
     */
    void* (*get_orig_operation)(
        struct kedr_coi_object* in_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data,
        const void* object,
        size_t operation_offset);
    
    /*
     * Called when 'get_orig_operation' is called for an object which
     * is watched.
     * 
     * Executed in atomic context.
     * 
     * Should return original object's operation at the given offset or
     * ERR_PTR() on error.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this.
     */
    void* (*get_orig_operation_nodata)(
        struct kedr_coi_object* in_iface_impl,
        const void* object,
        size_t operation_offset);

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
struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_create_indirect(
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements);

struct kedr_coi_instrumentor_normal*
kedr_coi_instrumentor_create_indirect1(
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
     * Executed in atomic context.
     * 
     * Operation should restore object operations and 'op_orig' should 
     * be set to original operation for object.
     * 
     * On any fail, function should return negative error code.
     * If function fails to extract original operation, 'op_orig'
     * should be set to ERR_PTR().
     * 
     * No other callback function which works with the same object or
     * prototype object may be called concurrently with this.
     *
     * NOTE: 'restore object operations' means that object operations
     * will work AS original ones, but they may be not equal to original
     * ones.
     */

    int (*restore_foreign_operations_nodata)(
        struct kedr_coi_object* if_iface_impl,
        const void* prototype_object,
        void* object,
        size_t operation_offset,
        void** op_orig);

    /*
     * Called when 'kedr_coi_bind_prototype_with_object' is called,
     * prototype_object is watched, but allocation of the data for the
     * object is failed.
     * 
     * Executed in atomic context.
     * 
     * Operation should restore object operations and 'op_orig' should 
     * be set to original operation for object.
     * 
     * On any fail, function should return negative error code.
     * If function fails to extract original operation, 'op_orig'
     * should be set to ERR_PTR().
     * 
     * No other callback function which works with the same object or
     * prototype object may be called concurrently with this.
     *
     * NOTE: 'restore object operations' means that object operations
     * will work AS original ones, but they may be not equal to original
     * ones.
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
     * Called when 'kedr_coi_bind_prototype_with_object' is called and
     * prototype_object is watched.
     * 
     * 'object_data_new' are data allocated for the object.
     * 
     * Executed in atomic context.
     * 
     * Operation should replace operations for an object and fill
     * 'object_data_new' with information about this replacements.
     * 'op_repl' should be set to replaced operation for an object.
     * 
     * If fail to replace operations, function should return negative
     * error code and set 'op_repl' to original operation of the object.
     * If original operation cannot be determined, 'op_repl' should be
     * set to ERR_PTR().
     * 
     * No other callback function which works with the same object or
     * prototype object may be called concurrently with this.
     */

    int (*replace_operations_from_prototype)(
        struct kedr_coi_object* iwf_iface_impl,
        struct kedr_coi_object* if_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data_new,
        void* object,
        struct kedr_coi_instrumentor_object_data* prototype_data,
        void* prototype_object,
        size_t operation_offset,
        void** op_repl);
    
    /*
     * Called when 'kedr_coi_bind_prototype_with_object' is called and
     * prototype_object is watched and object is watched also.
     * 
     * 'object_data' are data corresponded to the object.
     * 
     * Executed in atomic context.
     * 
     * Operation should update operations for an object.
     * 'op_repl' should be set to replaced operation for an object.
     * 
     * If fail to update operations, function should return negative
     * error code and set 'op_repl' to original operation of the object.
     * If original operation cannot be determined, 'op_repl' should be
     * set to ERR_PTR().
     * 
     * No other callback function which works with the same object or
     * prototype object may be called concurrently with this.
     */

    int (*update_operations_from_prototype)(
        struct kedr_coi_object* iwf_iface_impl,
        struct kedr_coi_object* if_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data,
        void* object,
        struct kedr_coi_instrumentor_object_data* prototype_data,
        void* prototype_object,
        size_t operation_offset,
        void** op_repl);
    
    /*
     * Called when 'kedr_coi_bind_prototype_with_object' is called,
     * prototype_object is not watched but object is watched.
     * 
     * 'object_data' are data corresponded to the object.
     * 
     * Executed in atomic context.
     * 
     * This is strange situation. Operation may do anything with
     * object's operations that is applicable to that situation
     * from the view of replacement mechanism. E.g, update operations
     * in the object.
     *
     * Then 'op_chained' should be set to operation, which is intended
     * to execute after. It may be object's replaced operation, or
     * original one.
     * 
     * On any fail, function should return negative error code.
     * If function fails to extract needed operation, 'op_chained'
     * should be set to ERR_PTR().
     * 
     * No other callback function which works with the same object or
     * prototype object may be called concurrently with this.
     */

    int (*chain_operation)(
        struct kedr_coi_object* iwf_iface_impl,
        struct kedr_coi_object* if_iface_impl,
        struct kedr_coi_instrumentor_object_data* object_data,
        void* object,
        void* prototype_object,
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

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect1(
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