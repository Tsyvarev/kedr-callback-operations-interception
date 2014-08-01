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

/* 
 * Array of replacements ends with element, which @operation_offset field is -1.
 * 
 * This is common way for iterate over such array.
 */
#define for_each_replacement(__it, __arr) for(__it = __arr; __it->operation_offset != -1; ++__it)

/*
 * Type of callback to be called for every object, which is watched
 * at time instrumentor is destroyed.
 */
typedef void (*trace_unforgotten_watch_t)(const void* object, void* user_data);
//*************Structure of normal instrumentor*************************
struct instrument_data_operations;

/* Abstract data described instrumentation of one operations object. */
struct instrument_data
{
    int refs;
    
    const struct instrument_data_operations* i_ops;
};

/* 
 * Way for search instrument_data by operations pointer.
 * 
 * Same instrument_data objects may embed elements from several hash tables.
 */
struct instrument_data_search
{
    struct kedr_coi_hash_elem ops_elem;
    /* Pointer to the object. */
    struct instrument_data* idata;
    /* 
     * Element in global hash table. Used for detect attempts for
     * use another instrumentor for already instrumented operations.
     */
    struct kedr_coi_hash_elem ops_elem_global;
};

/* Data described one watch for the interceptor. */
struct kedr_coi_instrumentor_watch_data
{
    /* Element of the hash table of objects. */
    struct kedr_coi_hash_elem object_elem;
    /* Referenced instrument_data object */
    struct instrument_data* idata;
};

struct kedr_coi_instrumentor
{
    /* Elements of that table are instrument_data_search. */
    struct kedr_coi_hash_table idata_table;
    
    // Hash table of watches, identificators are objects
    struct kedr_coi_hash_table objects_table;

    size_t operations_struct_size;
    
    const struct kedr_coi_replacement* replacements;
    
    bool (*replace_at_place)(const void* ops);
    
    /* 
     * There is no reason for two foreign instrumentors to watch for
     * the same object with operations.
     * 
     * In any case, when 'bind' will be called, only one of that watch
     * (the latest one) will have effect.
     * 
     * We store ops_p pointers for all foreign watches.
     * 
     * Note, that normal and foreign watching for same object is not
     * prohibited. Moreover, it is used for 'vm_ops' operations field
     * (type 'vm_operations_struct') in vm_area_struct.
     */
    struct kedr_coi_hash_table foreign_ops_p_table;
    
    /* Protect all hash tables, own and ones for foreign instrumentors. */
    spinlock_t lock;
};

/* 
 * Return operations, which should be used for instrumentation has an
 * effect.
 */
void* instrument_data_get_repl_operations(struct instrument_data* idata);

/* 
 * Return operations, which should be used for instrumentation has no
 * effect.
 */
void* instrument_data_get_orig_operations(struct instrument_data* idata);

/*
 * Return original operation at given offset.
 */
void* instrument_data_get_orig_operation(struct instrument_data* idata,
    size_t operation_offset);

/* Return 1 if operations are processed by given @idata object. */
bool instrument_data_my_operations(struct instrument_data* idata,
    const void* ops);

/*
 * Increment reference counter on idata.
 */
void instrument_data_ref(
    struct instrument_data* idata);

/*
 * Decrement reference counter on idata.
 * 
 * If it drops to 0, idata will be deleted. In that case operations
 * in the originial operations structure will be restored if needed.
 */
void instrument_data_unref(
    struct kedr_coi_instrumentor* instrumentor,
    struct instrument_data* idata);

/*
 * Decrement reference counter on idata.
 * 
 * If it drops to 0, idata will be deleted. Operations structure will not
 * be restored.
 */
void instrument_data_unref_norestore(
    struct kedr_coi_instrumentor* instrumentor,
    struct instrument_data* idata);

/* 
 * Return referenced 'instrument_data' object for given ops.
 * 
 * 'instrument_data' object will be created if needed.
 * 
 * On error, return ERR_PTR().
 */
struct instrument_data* instrumentor_get_data(
    struct kedr_coi_instrumentor* instrumentor,
    const void* ops);

/* 
 * Return idata object for given operations.
 * If such object is not exists, return NULL.
 */
struct instrument_data* instrumentor_find_data(
    struct kedr_coi_instrumentor* instrumentor,
    const void* ops);


//*************API for normal instrumentor*************************
/* Create instrumentor. */
struct kedr_coi_instrumentor* kedr_coi_instrumentor_create(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements,
    bool (*replace_at_place)(const void* ops));

/* 
 * Destroy instrumentor.
 */
void kedr_coi_instrumentor_destroy(struct kedr_coi_instrumentor* instrumentor,
    trace_unforgotten_watch_t trace_unforgotten_watch,
    void* user_data);


/* 
 * Watch for given object with given (indirect) operations.
 * 
 * Return 0 on success, 1 if already watched, negative error on fail.
 */
int kedr_coi_instrumentor_watch(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    const void** ops_p);


/* 
 * Forget about given object.
 * 
 * If ops_p is not NULL, this will be set to the original operations
 * (and that operations will be restored if needed).
 * 
 * Return 0 on success, 1 if not watched, negative error on fail.
 */
int kedr_coi_instrumentor_forget(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    const void** ops_p);

/* 
 * Return original operation at given offset for given object.
 * Return true on success and set @op_orig.
 * 
 * 'ops' is used if no watch for given object is found.
 * In that case 1 is returned and @op_orig is also set.
 * 
 * On error return negative error code, @op_orig is not set in that case.
 */
int kedr_coi_instrumentor_get_orig_operation(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void* ops,
    size_t operation_offset,
    void** op_orig);

/* 
 * Similar methods, but for directly watched object, which is also a
 * container of operations.
 */
int kedr_coi_instrumentor_watch_direct(
    struct kedr_coi_instrumentor* instrumentor,
    void* object);

int kedr_coi_instrumentor_forget_direct(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    bool norestore);

//**************Foreign instrumentor************************//
struct instrument_data_foreign_operations;

/* Abstract data described instrumentation of one operations object. */
struct instrument_data_foreign
{
    int refs;
    
    struct instrument_data* idata_binded;
    
    const struct instrument_data_foreign_operations* fi_ops;
};

/* Data described one watch for the foreign interceptor. */
struct kedr_coi_foreign_instrumentor_watch_data
{
    /* Referenced instrument_data_foreign object */
    struct instrument_data_foreign* idata_foreign;
    /* Element of the hash table of ids. */
    struct kedr_coi_hash_elem id_elem;
    /* Element of the hash table of ties. */
    struct kedr_coi_hash_elem tie_elem;
    /* Element in 'foreign_ops_p_table' in normal instrumentor. */
    struct kedr_coi_hash_elem ops_p_elem;
};

struct kedr_coi_foreign_instrumentor
{
    // Hash table of identificators
    struct kedr_coi_hash_table ids_table;
    // Hash table of ties. NOTE: may be not unique
    struct kedr_coi_hash_table ties_table;

    struct kedr_coi_instrumentor* instrumentor_binded;
    
    const struct kedr_coi_replacement* replacements;
};

/* 
 * Return operations, which should be used for instrumentation has an
 * effect.
 */
void* instrument_data_foreign_get_repl_operations(
    struct instrument_data_foreign* idata_foreign);


/* 
 * Return referenced 'instrument_data' object for given ops.
 * 
 * 'instrument_data_foreign' object will be created if needed.
 * 
 * On error, return ERR_PTR().
 */
struct instrument_data_foreign* instrumentor_foreign_get_data(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* ops);

/* 
 * Return foreign idata object for given operations.
 * If such object is not exists, return NULL.
 */
struct instrument_data_foreign* instrumentor_foreign_find_data(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* ops);


/*
 * Increment reference counter on idata_foreign.
 */
void instrument_data_foreign_ref(
    struct instrument_data_foreign* idata_foreign);
/*
 * Decrement reference counter on idata_foreign.
 * 
 * If it drops to 0, idata will be deleted. In that case operations
 * in the originial operations structure will be restored if needed.
 */
void instrument_data_foreign_unref(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign);

/*
 * Decrement reference counter on idata_foreign.
 * 
 * If it drops to 0, idata will be deleted. Operations structure will not
 * be restored.
 */
void instrument_data_foreign_unref_norestore(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign);


/*
 * Return normal operation at given offset.
 * 
 * It may be a replacement of the normal instrumentor, or original
 * operation iself (which, in turn, may be NULL).
 * 
 * This value is used for @op_chained in regular 'bind_with_factory' path.
 */
void* instrument_data_foreign_normal_operation(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign,
    size_t operation_offset);

/*
 * Chain operation at given offset.
 * 
 * It may be a replacement of the normal instrumentor, or original
 * operation iself (which, in turn, may be NULL).
 * 
 * This value is used for @op_chained when 'bind_with_factory' shouldn't
 * bind object.
 */
void* instrument_data_foreign_chain_operation(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign,
    size_t operation_offset);


//***********  API for foreign instrumentor************************
/* Create foreign instrumentor, binded with given normal one. */
struct kedr_coi_foreign_instrumentor* kedr_coi_foreign_instrumentor_create(
    struct kedr_coi_instrumentor* instrumentor_binded,
    const struct kedr_coi_replacement* replacements);

/* 
 * Destroy foreign instrumentor.
 */
void kedr_coi_foreign_instrumentor_destroy(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    trace_unforgotten_watch_t trace_unforgotten_watch,
    void* user_data);
    
/* 
 * Watch for given object(id).
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

/* 
 * If 'foriegn_tie' is watched by foreign instrumentor,
 * watch for 'object' by the normal instrumentor(with which foreign one is binded).
 * 
 * Also set 'op_chained' to operation which should be executed next.
 * 
 * If 'foreign_tie' is watched and 'object' become watched, replace
 * operations 'as if' them are watched by the normal instrumentor, set
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
 * If 'foreign_tie' is not watched, or 'object' is already watched
 * by the normal instrumentor, set 'op_chained' to operation which
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
    void* object,
    const void* foreign_tie,
    const void** ops_p,
    size_t operation_offset,
    void** op_chained,
    void** op_orig);


/*************** instrumentation mechanism implementation *************/
/*
 * All callbacks(normal and foreign ones) are called under
 * instrumentor's lock.
 */

struct instrument_data_operations
{
    /* 
     * Return pointer to operations, which should be used for
     * instrumentation mechanism works.
     */
    void* (*get_repl_operations)(struct instrument_data* idata);
    
    /* 
     * Return pointer to original operations structure.
     */
    void* (*get_orig_operations)(struct instrument_data* idata);


    /* Return original operation at given offset. */
    void* (*get_orig_operation)(
        struct instrument_data* idata,
        size_t operation_offset);
    
    
    /*
     * Return 1 if operations are processed by this @idata object.
     * Return 0 otherwise.
     * 
     * Used for detect changed operations when update watch.
     * */
    bool (*my_operations)(struct instrument_data* idata,
        const void* ops);
    
    void (*destroy_idata)(
        struct kedr_coi_instrumentor* instrumentor,
        struct instrument_data* idata);
    
    void (*destroy_idata_norestore)(
        struct kedr_coi_instrumentor* instrumentor,
        struct instrument_data* idata);
    

    /* 
     * Create foreign data for given ones.
     * 
     * On success, reference on idata will be consumed for object created.
     * 
     * Return ERR_PTR() on error.
     */
    struct instrument_data_foreign* (*create_foreign_data)(
        struct instrument_data* idata,
        struct kedr_coi_foreign_instrumentor* instrumentor_foreign);

    /* 
     * Look for foreign data for given ones.
     * 
     * If not found, NULL is be returned.
     */
    struct instrument_data_foreign* (*find_foreign_data)(
        struct instrument_data* idata,
        struct kedr_coi_foreign_instrumentor* instrumentor_foreign);

};

struct instrument_data_foreign_operations
{
    /* 
     * Return pointer to operations, which should be used for
     * instrumentation mechanism works.
     */
    void* (*get_repl_operations)(
        struct instrument_data_foreign* idata_foreign);

    /* 
     * Return 'normal' operation at given offset.
     * 
     * It may be replacement operation for normal instrumentor,
     * or original operation itself.
     */
    void* (*get_normal_operation)(
        struct kedr_coi_foreign_instrumentor* instrumentor,
        struct instrument_data_foreign* idata_foreign,
        size_t operation_offset);

    /* 
     * If there are other foriegn instrumentation is done for these ops,
     * return replacement for the operation at given offset.
     * Otherwise return original operation.
     */
    void* (*get_chain_operation)(
        struct kedr_coi_foreign_instrumentor* instrumentor,
        struct instrument_data_foreign* idata_foreign,
        size_t operation_offset);
        
    
    void (*destroy_idata_foreign)(
        struct kedr_coi_foreign_instrumentor* instrumentor,
        struct instrument_data_foreign* idata_foreign);
    
    void (*destroy_idata_foreign_norestore)(
        struct kedr_coi_foreign_instrumentor* instrumentor,
        struct instrument_data_foreign* idata_foreign);
    
};

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
