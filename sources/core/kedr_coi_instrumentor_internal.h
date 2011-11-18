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

//*************Structure of normal instrumentor*************************
struct kedr_coi_instrumentor_watch_data;

struct kedr_coi_instrumentor_impl;
struct kedr_coi_instrumentor_impl_iface;

struct kedr_coi_instrumentor_type;
/* Instrumentor as virtual object */
struct kedr_coi_instrumentor
{
    // All fields - protected
    
    // Hash table of watches, identificators are objects
    struct kedr_coi_hash_table hash_table_objects;
    // Protect hash table of watches
    spinlock_t lock;
    
    // used only while instrumentor is being deleted
    void (*trace_unforgotten_watch)(void* object, void* user_data);
    void* user_data;
   
    // Virtual destructor
    void (*destroy)(struct kedr_coi_instrumentor* instrumentor);
    
    struct kedr_coi_instrumentor_impl* iface_impl;
};

struct kedr_coi_instrumentor_impl
{
    struct kedr_coi_instrumentor_impl_iface* interface;
};

// Data corresponded to one replacement(one watch() call)
struct kedr_coi_instrumentor_watch_data
{
    // All fields - protected
    struct kedr_coi_hash_elem hash_elem_object;
};


// Interface of the implementation of concrete instrumentor
struct kedr_coi_instrumentor_impl_iface
{
    /*
     * Called when 'watch' is called.
     * 
     * Should return watch_data or NULL on error. May sleep.
     */
    struct kedr_coi_instrumentor_watch_data* (*alloc_watch_data)(
        struct kedr_coi_instrumentor_impl* iface_impl);

    /*
     * Called when 'watch' is called for a new object.
     * 
     * 'watch_data_new' are data allocated with 'alloc_watch_data'.
     * 
     * Executed in atomic context.
     * 
     * Should replace operations and fill watch_data_new with data
     * described this watch.
     * 
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data_new' becomes
     * available for other functions only when this callback returns
     * successfully.
     */
    int (*replace_operations)(
        struct kedr_coi_instrumentor_impl* iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data_new,
        const void** ops_p);
    
    /*
     * Called when 'forget' is called for watched object.
     * 
     * 'watch_data' are data concerned with that object.
     * 
     * Executed in atomic context.
     * 
     * Should remove from watch data information about replacement and,
     * if ops_p is not NULL, set it to initial operations.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data' become unavailable
     * for other functions after this callback returns.
     */
    void (*clean_replacement)(
        struct kedr_coi_instrumentor_impl* iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data,
        const void** ops_p);
    
    /*
     * Called after replace_operations() was failed, after
     * clean_replacement() is called or after update_operations()
     * returns 1.
     * 
     * Should free data previously allocated with alloc_watch_data().
     */
    void (*free_watch_data)(
        struct kedr_coi_instrumentor_impl* iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data);


    /*
     *  Called when 'watch' is called for id already used for watch.
     * 
     *  Should update watch_data and operations for continue watch.
     * 
     * If need to recreate watch_data, should restore operations and
     * return 1. After that, replace_operations() will be called for
     * newly created watch data.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data' become
     * unavailable for other functions after this callback returns 1.
     */
    int (*update_operations)(
        struct kedr_coi_instrumentor_impl* iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data,
        const void** ops_p);
    
    /*
     * Called when 'get_orig_operation' is called for an object which
     * is watched.
     * 
     * 'watch_data' are data(any) corresponded to that object.
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
        struct kedr_coi_instrumentor_impl* iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data,
        const void* ops,
        size_t operation_offset);
    
    /*
     * Called when 'get_orig_operation' is called for an object which
     * is not watched.
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
        struct kedr_coi_instrumentor_impl* iface_impl,
        const void* ops,
        size_t operation_offset);

    /*
     * Called when instrumentor was destroyed.
     */
    void (*destroy_impl)(struct kedr_coi_instrumentor_impl* iface_impl);
};


//************* Instrumentor API ********************//

int kedr_coi_instrumentor_watch(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void** ops_p);

int kedr_coi_instrumentor_forget(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void** ops_p);

void kedr_coi_instrumentor_destroy(
    struct kedr_coi_instrumentor* instrumentor,
    void (*trace_unforgotten_watch)(void* object, void* user_data),
    void* user_data);

int
kedr_coi_instrumentor_get_orig_operation(
    struct kedr_coi_instrumentor* instrumentor,
    const void* tie,
    const void* ops,
    size_t operation_offset,
    void** op_orig);


// For create instrumentors of concrete type
struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create(
    struct kedr_coi_instrumentor_impl* iface_impl);


//********** Creation of the normal instrumentors ********************
/*
 * Instrumentor expects that pointer to operations may change, but
 * content of operations struct may not change.
 */

struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_indirect(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements);


struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_indirect1(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements);


/*
 * Instrumentor expects that pointer to operations may not be changed
 * and unique, but content of operations struct may change.
 */
struct kedr_coi_instrumentor*
kedr_coi_instrumentor_create_direct(
    size_t object_struct_size,
    const struct kedr_coi_replacement* replacements);

//**************Foreign instrumentor************************//
struct kedr_coi_instrumentor_with_foreign;

struct kedr_coi_instrumentor_with_foreign_impl_iface;

struct kedr_coi_foreign_instrumentor_watch_data;

struct kedr_coi_foreign_instrumentor_impl;
struct kedr_coi_foreign_instrumentor_impl_iface;

struct kedr_coi_foreign_instrumentor_type;
/* Instrumentor as virtual object */
struct kedr_coi_foreign_instrumentor
{
    struct kedr_coi_instrumentor_with_foreign* instrumentor_binded;
    // Hash table of identificators
    struct kedr_coi_hash_table hash_table_ids;
    // Hash table of ties. NOTE: may be not unique
    struct kedr_coi_hash_table hash_table_ties;
    // Protect both hash tables, 'ids' and 'ties'
    spinlock_t lock;
    
    // used only while instrumentor is being deleted
    void (*trace_unforgotten_watch)(void* id, void* tie, void* user_data);
    void* user_data;
   
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl;
};

// Data corresponded to one replacement(one watch() call)
struct kedr_coi_foreign_instrumentor_watch_data
{
    // All fields - protected
    struct kedr_coi_hash_elem hash_elem_id;
    struct kedr_coi_hash_elem hash_elem_tie;
};

struct kedr_coi_foreign_instrumentor_impl
{
    struct kedr_coi_foreign_instrumentor_impl_iface* interface;
};
// Interface of the implementation of concrete instrumentor
struct kedr_coi_foreign_instrumentor_impl_iface
{
    /*
     * Called when 'watch' is called.
     * 
     * Should return watch_data or NULL on error. May sleep.
     */
    struct kedr_coi_foreign_instrumentor_watch_data* (*alloc_watch_data)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl);

    /*
     * Called when 'watch' is called for a new id.
     * 
     * 'watch_data_new' are data allocated with 'alloc_watch_data'.
     * 
     * Executed in atomic context.
     * 
     * Should replace operations and fill watch_data_new with data
     * described this watch.
     * 
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data_new' become
     * available for other functions only when this callback returns
     * successfully.
     */
    int (*replace_operations)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_foreign_instrumentor_watch_data* watch_data_new,
        const void** ops_p);
    
    /*
     * Called when 'forget' is called for an id, which is watched.
     * 
     * 'watch_data' are data concerned with this id.
     * 
     * Executed in atomic context.
     * 
     * Should remove from watch data information about replacement and,
     * if ops_p is not NULL, set it to initial operations.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data' become unavailable
     * for other functions after this callback returns.
     */
    void (*clean_replacement)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
        const void** ops_p);
    
    /*
     * Called after replace_operations() was failed, after
     * clean_replacement() is called or after update_operations()
     * returns 1.
     * 
     * Should free data previously allocated with alloc_watch_data().
     */
    void (*free_watch_data)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_foreign_instrumentor_watch_data* watch_data);


    /*
     *  Called when 'watch' is called for id already used for watch.
     * 
     *  Should update watch_data and operations for continue watch.
     * 
     * If need to recreate watch_data, should restore operations and
     * return 1. After that, replace_operations() will be called for
     * newly created watch data.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data' become
     * unavailable for other functions after this callback returns 1.
     */
    int (*update_operations)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
        const void** ops_p);
    
    /*
     * Called when 'kedr_coi_bind_object' is called,
     * but neither foreign tie nor normal object is watched.
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
     * foreign tie may be called concurrently with this.
     *
     * NOTE: 'restore object operations' means that object operations
     * will work AS original ones, but they may be not equal to original
     * ones.
     */

    int (*restore_foreign_operations_nodata)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        const void** ops_p,
        size_t operation_offset,
        void** op_orig);

    /*
     * Called when 'kedr_coi_bind_object' is called, foreign tie is
     * watched, but allocation of the data for the new watch is failed.
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
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
        const void** ops_p,
        size_t operation_offset,
        void** op_orig);

    /*
     * Called when instrumentor was destroyed.
     */
    void (*destroy_impl)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl);
};

struct kedr_coi_instrumentor_with_foreign_impl_iface
{
    struct kedr_coi_instrumentor_impl_iface base;
    // Factory method
    struct kedr_coi_foreign_instrumentor_impl*
        (*foreign_instrumentor_impl_create)(
            struct kedr_coi_instrumentor_impl* iface_wf_impl,
            const struct kedr_coi_replacement* replacements);

    /*
     * Called when 'kedr_coi_bind_object' is called and tie object is
     * watched.
     * 
     * 'watch_data_new' are data allocated for the object.
     * 
     * Executed in atomic context.
     * 
     * Operation should replace operations for an object and fill
     * 'watch_data_new' with information about this replacements.
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

    int (*replace_operations_from_foreign)(
        struct kedr_coi_instrumentor_impl* iface_wf_impl,
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data_new,
        const void** ops_p,
        struct kedr_coi_foreign_instrumentor_watch_data* foreign_watch_data,
        size_t operation_offset,
        void** op_repl);
    
    /*
     * Called when 'kedr_coi_bind_object' is called, foreign tie is
     * watched and object is watched also.
     * 
     * 'watch_data' are data corresponded to the object.
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

    int (*update_operations_from_foreign)(
        struct kedr_coi_instrumentor_impl* iface_wf_impl,
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data,
        const void** ops_p,
        struct kedr_coi_foreign_instrumentor_watch_data* foreign_data,
        size_t operation_offset,
        void** op_repl);
    
    /*
     * Called when 'kedr_coi_bind_object' is called, tie is not watched
     * but object is watched.
     * 
     * 'watch_data' are data corresponded to the object.
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
     * tie may be called concurrently with this.
     */

    int (*chain_operation)(
        struct kedr_coi_instrumentor_impl* iface_wf_impl,
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data,
        const void** ops_p,
        size_t operation_offset,
        void** op_chained);
};

struct kedr_coi_instrumentor_with_foreign
{
    struct kedr_coi_instrumentor base;
    
    struct kedr_coi_instrumentor_impl* iface_wf_impl;
};

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create(
    struct kedr_coi_instrumentor_impl* iface_wf_impl);

//***********  API for foreign instrumentor************************

int kedr_coi_foreign_instrumentor_watch(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* id,
    const void* tie,
    const void** ops_p);

int kedr_coi_foreign_instrumentor_forget(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* id,
    const void** ops_p);

void kedr_coi_foreign_instrumentor_destroy(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    void (*trace_unforgotten_watch)(void* id, void* tie, void* user_data),
    void* user_data);

int kedr_coi_foreign_instrumentor_bind(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* object,
    const void* tie,
    const void** ops_p,
    size_t operation_offset,
    void** op_chained);


//*********** Creation of foreign instrumentor ************************
struct kedr_coi_foreign_instrumentor*
kedr_coi_foreign_instrumentor_create(
    struct kedr_coi_instrumentor_with_foreign* instrumentor,
    const struct kedr_coi_replacement* replacements);

/*
 * Create indirect instrumentor with support of foreign instrumentors.
 */
struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements);

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect1(
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