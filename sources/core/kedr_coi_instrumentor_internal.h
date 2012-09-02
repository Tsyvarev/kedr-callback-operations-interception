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
    replace_null,     /* Replace only if operation is NULL */
    replace_not_null, /* Replace only if operation is not NULL */
    replace_all       /* Replace in any case */
};

struct kedr_coi_replacement
{
    size_t operation_offset;
    void* repl;
    enum replacement_mode mode;
};

//*************Structure of normal instrumentor*************************
struct kedr_coi_instrumentor_watch_data;

struct kedr_coi_instrumentor_impl;
struct kedr_coi_instrumentor_impl_iface;

struct kedr_coi_instrumentor_type;
/* Instrumentor as abstract object */
struct kedr_coi_instrumentor
{
    // All fields - protected
    
    // Hash table of watches, identificators are objects
    struct kedr_coi_hash_table hash_table_objects;
    // Protect hash table of watches
    spinlock_t lock;
    
    // Virtual destructor
    void (*destroy)(struct kedr_coi_instrumentor* instrumentor);
    
    /* Backend which implements replacement mechanism. */
    struct kedr_coi_instrumentor_impl* iface_impl;

    // used only while instrumentor is being deleted
    void (*trace_unforgotten_watch)(void* object, void* user_data);
    void* user_data;
};

// Data corresponded to one replacement(one watch() call)
struct kedr_coi_instrumentor_watch_data
{
    // All fields - protected
    struct kedr_coi_hash_elem hash_elem_object;
};


//************* Instrumentor API ********************//
/* 
 * Watch for given object.
 * 
 * All needed operations of that object are replaced corresponded to
 * backend.
 * 
 * If object is already watched, update operations replacement.
 * Return 0 on success, negative error code on fail.
 */
int kedr_coi_instrumentor_watch(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void** ops_p);

/* 
 * Cancel watching for given object.
 * 
 * If ops_p is not NULL, operations replacement should be rollbacked.
 * 
 * Return 0 on success, 1 if object wasn't watched.
 */
int kedr_coi_instrumentor_forget(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void** ops_p);

/*
 * Destroy instrumentor and its backend.
 */
void kedr_coi_instrumentor_destroy(
    struct kedr_coi_instrumentor* instrumentor,
    void (*trace_unforgotten_watch)(void* object, void* user_data),
    void* user_data);


/* 
 * Determine original operation for watched object(tie).
 * 
 * On success set 'op_orig' to original operation and return 0.
 * If object isn't watched, set 'op_orig' to original operation
 * and return 1.
 * This is normal situation for backends which use agressive replacement
 * mechanism, which may replace operations not only for watched object,
 * but also for other objects.
 * 
 * If object isn't watched and original operation may not be restored,
 * return negative error code. Usually, this means unrecoverable error.
 */
int kedr_coi_instrumentor_get_orig_operation(
    struct kedr_coi_instrumentor* instrumentor,
    const void* tie,
    const void* ops,
    size_t operation_offset,
    void** op_orig);


/****************** Backend for instrumentor **************************/
/* Implements replacement mechanism. */
struct kedr_coi_instrumentor_impl
{
    /*
     * Destroy backend.
     */
    void (*destroy_impl)(struct kedr_coi_instrumentor_impl* iface_impl);

    struct kedr_coi_instrumentor_impl_iface* interface;
};

/* Interface of the instrumentor backend */
struct kedr_coi_instrumentor_impl_iface
{
    /*
     * Allocate new watch data object.
     * 
     * Return object allocated or NULL on error. Can sleep.
     */
    struct kedr_coi_instrumentor_watch_data* (*alloc_watch_data)(
        struct kedr_coi_instrumentor_impl* iface_impl);

    /*
     * Called when 'watch' is called for a new object.
     * 
     * 'watch_data_new' are data allocated with 'alloc_watch_data'.
     * 
     * Replace operations and fill 'watch_data_new' with data
     * described this replacement.
     * 
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data_new' becomes
     * available for other functions only when this callback returns
     * successfully.
     *
     * Executed in atomic context.
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
     * Remove information about replacement from 'watch_data' and,
     * if 'ops_p' is not NULL, adjust 'ops_p' to initial operations.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data' become unavailable
     * for other functions after this callback returns.
     * 
     * Executed in atomic context.
     */
    void (*clean_replacement)(
        struct kedr_coi_instrumentor_impl* iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data,
        const void** ops_p);
    
    /*
     * Free data previously allocated with alloc_watch_data().
     * 
     * 'watch_data' has no replacement information at this stage.
     */
    void (*free_watch_data)(
        struct kedr_coi_instrumentor_impl* iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data);


    /*
     *  Called when 'watch' is called for id already used for watch.
     * 
     *  Update 'watch_data' and operations for continue watch.
     * 
     * If need to recreate watch_data, should restore operations and
     * return 1. After that, replace_operations() will be called for
     * newly created watch data.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data' become
     * unavailable for other functions after this callback returns 1.
     * 
     * Executed in atomic context.
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
     * Return original object's operation at the given offset.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this.
     *
     * Executed in atomic context.
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
     * Should return original object's operation at the given offset or
     * ERR_PTR() on error(normally, caller cannot recover from that error).
     *
     * No other callback function which works with the same object may
     * be called concurrently with this.
     *
     * Executed in atomic context.
     */
    void* (*get_orig_operation_nodata)(
        struct kedr_coi_instrumentor_impl* iface_impl,
        const void* ops,
        size_t operation_offset);
};


/* 
 * Create instrumentors with given backend.
 * 
 * Backend is controlled by instrumentor created.
 */
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

/*
 * Same as previous function, but use more agressive replacement mechanism.
 * 
 * This mechanism is tolerable to restoring operations pointer outside
 * of instrumentor, but replacements also affects on other objects
 * which share same operations structure.
 */
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

    /* Backend */
    struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl;
};

// Data corresponded to one replacement(one watch() call)
struct kedr_coi_foreign_instrumentor_watch_data
{
    // All fields - protected
    struct kedr_coi_hash_elem hash_elem_id;
    struct kedr_coi_hash_elem hash_elem_tie;
};

//***********  API for foreign instrumentor************************

/* 
 * Watch for given object(tie).
 * 
 * All needed operations of that object are replaced corresponded to
 * backend.
 * 
 * If object has been already watched, update watching. 
 * 
 * Return 0 on success, negative error code on fail.
 * 
 */
int kedr_coi_foreign_instrumentor_watch(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* id,
    const void* tie,
    const void** ops_p);

/* 
 * Cancel watching for given object.
 * 
 * If ops_p is not NULL, operations replacement should be rollbacked.
 * 
 * Return 0 on success, 1 if object wasn't watched.
 */
int kedr_coi_foreign_instrumentor_forget(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* id,
    const void** ops_p);

/* Destroy instrumentor and its backend. */
void kedr_coi_foreign_instrumentor_destroy(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    void (*trace_unforgotten_watch)(void* id, void* tie, void* user_data),
    void* user_data);

/* 
 * If 'foriegn_tie' is watched by foreign instrumentor,
 * watch for 'tie' of normal instrumentor(with which foreign one is binded).
 * 
 * Also set 'op_chained' to operation which should be executed next.
 * 
 * If 'foreign_tie' is watched and 'tie' become watched, replace
 * operations 'as if' them is watched by normal instrumentor, set
 * 'op_chained' to operation which should be called next,
 * 'op_orig' to original operation, and return 0.
 * 
 * NOTE: Do not confuse 'op_chained' and 'op_orig':
 * later is operation which would be without KEDR COI at all,
 * former may be replacement of the normal instrumentor.
 * 
 * NOTE: 'As if' in operations replacement means that futher calls to
 * operations produce same result as being watched by normal instrumentor.
 * But these operations may differ from ones would be when replaced
 * solely by normal instrumentor(see below).
 * 
 * If 'foreign_tie' is not watched, set 'op_chained' to operation which
 * should be called next and return 1.
 * 
 * If 'foreign_tie' is watched, but watching for the new object is failed
 * (e.g. in case of memory pressure), set 'op_chained' to operation
 * which should be called next and return negative error code.
 * 
 * If cannot determine chained operation, return negative error code and
 * set op_chained to ERR_PTR(). Usually, this is an unrecoverable error.
 * 
 * In short:
 * - returning 0 means 'everything is OK', 'op_orig' is set.
 * - returning 1 means 'should be ignored'(simply execute 'op_chained')
 * - returning negative error code means error, recoverable or not
 * depending on 'op_chained'.
 * 
 * NOTE: NULL stored in 'op_chained' means that original operation is
 * NULL and some default actions should be executed.
 */
int kedr_coi_foreign_instrumentor_bind(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* tie,
    const void* foreign_tie,
    const void** ops_p,
    size_t operation_offset,
    void** op_chained,
    void** op_orig);


/*******************Backend for foreign instrumentor*******************/
/* Implements replacement mechanism */
struct kedr_coi_foreign_instrumentor_impl
{
    /*
     * Destroy backend.
     */
    void (*destroy_impl)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl);

    struct kedr_coi_foreign_instrumentor_impl_iface* interface;
};
// Interface of the foreign instrumentor backend
struct kedr_coi_foreign_instrumentor_impl_iface
{
    /*
     * Allocate watch_data object.
     * 
     * Return object allocated or NULL on error. May sleep.
     */
    struct kedr_coi_foreign_instrumentor_watch_data* (*alloc_watch_data)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl);

    /*
     * Called when 'watch' is called for a new id.
     * 
     * 'watch_data_new' are data allocated with 'alloc_watch_data'.
     * 
     * Replace operations and fill 'watch_data_new' with data
     * described this replacement.
     * 
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data_new' become
     * available for other functions only when this callback returns
     * successfully.
     * 
     * Executed in atomic context.
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
     * if 'ops_p' is not NULL, adjust 'ops_p' to initial operations.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data' become unavailable
     * for other functions after this callback returns.
     *
     * Executed in atomic context.
     */
    void (*clean_replacement)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
        const void** ops_p);
    
    /*
     * Free data previously allocated with alloc_watch_data().
     */
    void (*free_watch_data)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_foreign_instrumentor_watch_data* watch_data);


    /*
     * Called when 'watch' is called for id already used for watch.
     * 
     * Update watch_data and operations for continue watch.
     * 
     * If need to recreate watch_data, should restore operations and
     * return 1. After that, replace_operations() will be called for
     * newly created watch data.
     *
     * No other callback function which works with the same object may
     * be called concurrently with this. 'watch_data' become
     * unavailable for other functions after this callback returns 1.
     * 
     * Executed in atomic context.
     */
    int (*update_operations)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
        const void** ops_p);
    
    /*
     * Called when 'kedr_coi_bind_object' is called,
     * but neither foreign tie nor normal object is watched.
     * 
     * Restore object operations and 'op_orig' should be set to original
     * operation for object.
     * 
     * On any fail, function should return negative error code.
     * If function fails to extract original operation, 'op_orig'
     * should be set to ERR_PTR() (that error usually is unrecoverable for caller).
     * 
     * No other callback function which works with the same object or
     * foreign tie may be called concurrently with this.
     *
     * NOTE: 'restore object operations' means that object operations
     * will work AS original ones, but they may be not equal to original
     * ones.
     *
     * Executed in atomic context.
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
     * Restore object operations and 'op_orig' should 
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
     * 
     * Executed in atomic context.
     */

    int (*restore_foreign_operations)(
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_foreign_instrumentor_watch_data* watch_data,
        const void** ops_p,
        size_t operation_offset,
        void** op_orig);
};


/*****************Instrumentor with foreing support********************/
struct kedr_coi_instrumentor_with_foreign_impl;

/* Normal instrumentor which also may create foreign ones. */
struct kedr_coi_instrumentor_with_foreign
{
    /* Derivation from normal instrumentor */
    struct kedr_coi_instrumentor base;
    /* Backend */
    struct kedr_coi_instrumentor_with_foreign_impl* iface_wf_impl;
};


/********* API for normal instrumentor with foreign support ************/
/* Create foreign instrumentor for given one */
struct kedr_coi_foreign_instrumentor*
kedr_coi_foreign_instrumentor_create(
    struct kedr_coi_instrumentor_with_foreign* instrumentor,
    const struct kedr_coi_replacement* replacements);

/************ Backend for instrumentor with foreign support ***********/
/* Implements replacement mechanism */
struct kedr_coi_instrumentor_with_foreign_impl_iface;

struct kedr_coi_instrumentor_with_foreign_impl
{
    /* Derives from normal instrumentor backend */
    struct kedr_coi_instrumentor_impl base;
    /* Additional virtual functions */
    struct kedr_coi_instrumentor_with_foreign_impl_iface* interface;
};

struct kedr_coi_instrumentor_with_foreign_impl_iface
{
    /* 
     * All callback operations which accept 'op_orig'/'op_chained'
     * parameter are called (indirectly) from intermediate operation.
     * Operation from that parameter is used by intermediate operation
     * as operation to be called next.
     * 
     * If that operation cannot be determined, it should be set to
     * ERR_PTR() and error should be returned. Note, that this error
     * is usually cannot be recovered by the caller.
     */

    /*
     * Create foreign instrumentor for normal one.
     */
    struct kedr_coi_foreign_instrumentor_impl*
        (*foreign_instrumentor_impl_create)(
            struct kedr_coi_instrumentor_with_foreign_impl* iface_wf_impl,
            const struct kedr_coi_replacement* replacements);

    /*
     * Called when 'kedr_coi_bind_object' is called and tie object is
     * watched.
     * 
     * 'watch_data_new' are data allocated for the object.
     * 
     * Replace operations for an object and fill 'watch_data_new' with
     * information about this replacements.
     * 'op_repl' should be set to replaced operation for an object.
     * 
     * If fail to replace operations, function should return negative
     * error code and set 'op_repl' to original operation of the object.
     * 
     * No other callback function which works with the same object or
     * prototype object may be called concurrently with this.
     * 
     * Executed in atomic context.
     */

    int (*replace_operations_from_foreign)(
        struct kedr_coi_instrumentor_with_foreign_impl* iface_wf_impl,
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
     * Operation should update operations for an object.
     * 'op_repl' should be set to replaced operation for an object.
     * 
     * If fail to update operations, function should return negative
     * error code and set 'op_repl' to original operation of the object.
     * 
     * No other callback function which works with the same object or
     * prototype object may be called concurrently with this.
     * 
     * Executed in atomic context.
     */

    int (*update_operations_from_foreign)(
        struct kedr_coi_instrumentor_with_foreign_impl* iface_wf_impl,
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
     * 
     * No other callback function which works with the same object or
     * tie may be called concurrently with this.
     */

    int (*chain_operation)(
        struct kedr_coi_instrumentor_with_foreign_impl* iface_wf_impl,
        struct kedr_coi_foreign_instrumentor_impl* foreign_iface_impl,
        struct kedr_coi_instrumentor_watch_data* watch_data,
        const void** ops_p,
        size_t operation_offset,
        void** op_chained);
};

struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create(
    struct kedr_coi_instrumentor_with_foreign_impl* iface_wf_impl);

/*********** Creation of instrumentor with foreign support ************/
/*
 * Similar to kedr_coi_instrumentor_create_indirect(), but with foreign
 * support.
 */
struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements);

/*
 * Similar to kedr_coi_instrumentor_create_indirect1(), but with foreign
 * support.
 */
struct kedr_coi_instrumentor_with_foreign*
kedr_coi_instrumentor_with_foreign_create_indirect1(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements);

/* Foreign support for direct instrumentor is not needed(not used) */

/********************** Global functions ******************************/
/*
 * This function should be called before using any of the functions above.
 */
int kedr_coi_instrumentors_init(void);

/*
 * This function should be called when instrumentors are not needed.
 */
void kedr_coi_instrumentors_destroy(void);

#endif /* KEDR_COI_INSTRUMENTOR_INTERNAL_H */