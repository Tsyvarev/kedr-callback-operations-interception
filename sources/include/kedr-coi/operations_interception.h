/*
 * This file contains types and functions declarations to be used
 * for intercept callback operations of kernel objects, such as
 * files, inodes, dentry...
 * 
 * Standard use case:
 * 
 * 0. Determine, operations on which object type you need to intercept.
 * Interception of operation means that you can call your function(s)
 * before or after this operation.
 * Such functions will be futher referenced as pre- and post-handlers.
 * 
 * 1. Create interceptor (struct kedr_coi_interceptor)
 * for the given object type operations
 * (see section "Interceptor creation" below).
 * 
 * 2. Write payload (struct kedr_coi_payload) which should define
 * which operations you want to intercept and what pre- and/or post-
 * handlers should be called in that interception.
 * 
 * 3. Register payload for operations interceptor created at step 1
 * (kedr_coi_payload_register()).
 * 
 * 4. Then you need to determine when object interested your is created,
 * and call kedr_coi_interceptor_watch() at this moment. Depended on
 * how objects are created, there are several ways to do this:
 * 
 * a) some objects are created by top-level kernel functions, such as 
 * cdev_add() or register_filesystem(). You may use KEDR for intercept
 * these functions and call kedr_coi_interceptor_watch()
 * in pre- or post- handlers of them.
 * 
 * b) some objects are created in operations of another objects. E.g.
 * super blocks (struct super_block) are created in get_sb() operation
 * of file system type objects (struct file_system_type). So you need to
 * intercept operations of creator's objects, for which you should
 * repeat all steps from 1 of this description (perhaps, recursively).
 * 
 * c) there are objects, which are created internally by the kernel.
 * File (struct file) is an example of such object:
 * it is created internally when file is opened from user space,
 * and then its open() function is executed.
 * In that case call kedr_coi_interceptor_watch() from handler of open()
 * operation is meaningless: because newly created file is not watched
 * by the interceptor, its open() operation is not intercepted,
 * and handler for it is not called.
 * 
 * But it is known, that operations for file object are copied from
 * inode (struct inode) object when file is created. You can intercept
 * creation of file if you use interceptor for file operations in inode
 * object. Such interceptor is referred as foreign, because it allows to
 * intercept operations which are called not for the owner object, but
 * for another object. Foreign interceptor doesn't allow to set handlers
 * for these operations but allow to automatically watch for an new
 * object, which is created by copiing operations from object, which is
 * already watched by foreign interceptor.
 * 
 * Creation of foreign interceptors are described in section
 * "Foreign interceptor creation".
 * 
 * 5. Determine when objects interested your are destroyed, and call
 * kedr_coi_interceptor_forget() at that moment. As in step 4, ways for
 * doing this depends on the way haw objects are destroyed.
 * Cases a) and b) from step 4 are applicable for interception of
 * object destruction wihtout changes.
 * As for objects which are created internally, them are usually
 * destroyed after some of its operation. E.g., files objects are
 * destroyed after their release() method. So setting handler for this
 * operation is sufficient for intercept object destruction.
 * 
 * 6. Before the first object may be watched by the interceptor, one should
 * call kedr_coi_interceptor_start() for this interceptor. This call
 * will fix(prevent from changes) interception handlers of this
 * interceptor, and create some additional data used for interception.
 * Usually this function is called from target_load_callback() of KEDR
 * payload.
 * 
 * Similar, after last object is forgot one should call
 * kedr_coi_interceptor_stop() for allow to unload payload modules or
 * load another ones. Usually this function is called from
 * target_unload_callback of KEDR payload.
 * 
 * 
 * 
 * Interceptor creation.
 * 
 * For create interceptor for operations of object of some type, one
 * should:
 * 
 * a) describe "geometry" of the operations inside the object.
 * 
 * There are 2 types of such geometry:
 * 
 * 1) For objects which have pointer to struct with all operations as a
 * field it is needed to set offset of this field in the object struct
 * and size of operations struct.
 * 
 * E.g., for interceptor of file operations this values are
 * offsetof(struct file, f_op) and sizeof(struct file_operations).
 * 
 * 2) For objects which have pointers to every operation as a distinct
 * fields it is needed to set size of object struct itself.
 * 
 * E.g., for interceptor of file system type operations it is
 * sizeof(struct file_system_type).
 * 
 * b) write intermediate operations for all object operations which
 * is planned to intercept. This operations will be called instead of
 * corresponded original operation of the object.
 * 
 * Each such operation should locally allocate(e.g., on stack)
 * intermediate info object(type 'struct kedr_coi_intermediate_info')
 * and call kedr_coi_interceptor_get_intermediate_info() function for
 * fill it.
 * After filling, object will contain information about pre-handlers,
 * post-handlers, and pointer to the original operation.
 * Then, intermediate operation should call in order:
 * - pre handlers(if exists)
 * - original operation
 * - post handlers(if exist)
 * 
 * If any handler exist, before calling handlers intermediate operation
 * should locally allocate call info object (type
 * 'struct kedr_coi_operation_call_info') and fill it. This object should
 * be passed to the handlers.
 * 
 * If operation should return a value, result of original operation call
 * should be stored and returned by intermediate operation. Also, when
 * call post handlers, this value should be passed to them.
 * 
 * For some objects pointer to the original operation may be NULL.
 * In that case, intermediate operation should perform some default
 * actions instead of call of orignal operation. These actions should
 * have similar effect to those ones which performed by the kernel when
 * it found that object operation pointer is NULL.
 * 
 * Examples of default actions for file object(struct file):
 * - open() - set result to 0
 * - read() - set result to -EIO
 * - llseek() - call default_llseek() store its result
 *              as result of operation call.
 * 
 * Interceptors with the first type of geometry are created with
 * kedr_coi_interceptor_create(), with the second one -
 * kedr_coi_interceptor_create_direct().
 * 
 * 
 * 
 * Foreign interceptor creation.
 * 
 * For create foreign interceptor, one should:
 * 
 * a) describe placement of such operations inside object:
 * -offset of the field pointed to operations struct inside object struct
 * 
 * E.g. for operations on file created from inode this offset is
 * offsetof(struct inode, i_fop).
 * 
 * b) write intermediate operations for operations which are called just
 * after foreign object is created and its operations are copied from
 * initial object. Normally there is only one such operation.
 * E.g., if foreign object is file('struct file'), one have to write
 * only intermediate operation for file operation open().
 * 
 * Intermediate operation should firstly allocate pointer to the chained
 * operation and then call kedr_coi_bind_prototype_with_object(),
 * which aside from other work will set this pointer.
 * 
 * Then intermediate operation should call chained one and return
 * its result (if operation return any value).
 * 
 * Like an intermediate operation for standard interceptor,
 * intermediate operation for foreign interceptor should correctly
 * process the case when pointer to the object operation is NULL.
 * 
 * c) Choose 'normal' interceptor, which will be binded with foreign one.
 * 'Binded interceptor' means that every object will be automatically
 * watched by this interceptor if it is created by copiing operations
 * from prototype object, which is watched by foreign interceptor.
 * 
 * Foreign interceptor is created with 
 * kedr_coi_interceptor_create_foreign().
 */

#ifndef OPERATIONS_INTERCEPTION_H
#define OPERATIONS_INTERCEPTION_H

#include <linux/module.h>

/*
 * Operations interceptor.
 * 
 * It is responsible for intercept callback operations for objects of
 * particular types.
 * User-defined functions may be assigned for call before or after
 * any of these operations (pre- and post- handlers).
 * These handlers will be called whenever corresponded operation is 
 * executed for the object, which is marked as 'watched'.
 */
struct kedr_coi_interceptor;

/*
 * Information about original operation call,
 * which is passed to the handlers.
 */
struct kedr_coi_operation_call_info
{
    // Return address of the operation
    void* return_address;
    /*
     * If handler need to check object's operation, it should use
     * this pointer instead of operation field in the object itself.
     * 
     * The thing is that object's operations are replaced for
     * implement interception mechanizm.
     */
    void* op_orig;
};


/*
 * Handler which should be executed before callback operation.
 * 
 * If original operation has signature
 * ret_type (*)(arg_type1,..., arg_typeN)
 * 
 * then function should have signature
 * 
 * void (*)(arg_type1,..., arg_typeN, kedr_coi_operation_call_info*).
 */

struct kedr_coi_pre_handler
{
    // offset of the operation, for which handler is should be used.
    size_t operation_offset;
    // function to execute
    void* func;
    // Whether need to intercept even default operation(NULL-pointer)
    bool external;
};

// End mark in pre-handlers array
#define kedr_coi_pre_handler_end {.operation_offset = -1}

/*
 * Handler which should be executed after callback operation.
 * 
 * If original operation has signature
 * ret_type (*)(arg_type1,..., arg_typeN)
 * 
 * and ret_type is not 'void', then function should have signature
 * 
 * void (*)(arg_type1,..., arg_typeN, ret_type, kedr_coi_operation_call_info*).
 * 
 * If original operation has signature
 * void (*)(arg_type1,..., arg_typeN)
 * 
 * then function should have signature
 * 
 * void (*)(arg_type1,..., arg_typeN, kedr_coi_operation_call_info*).
 */

struct kedr_coi_post_handler
{
    // offset of the operation, for which handler is should be used.
    size_t operation_offset;
    // function to execute
    void* func;
    // Whether need to intercept even default operation(NULL-pointer)
    bool external;
};

// End mark in post-handlers array
#define kedr_coi_post_handler_end {.operation_offset = -1}


/*
 * Contain information about what object's operations
 * one want to intercept and how.
 */
struct kedr_coi_payload
{
    /* 
     * If not NULL, this module will be prevented to unload while
     * payload is in use.
     */
    struct module* mod;
    
    /* Array of pre-handlers ended with mark */
    struct kedr_coi_pre_handler* pre_handlers;
    /* Array of post-handlers ended with mark */
    struct kedr_coi_post_handler* post_handlers;
};


/* 
 * Register payload with the operations interceptor.
 * Handlers, enumerated in payload, will be executed whenever
 * corresponded callback operation is executed for any object, which is
 * watched by the interceptor(see below).
 * 
 * Returns 0 if successful, an error code otherwise.
 * 
 * This function is usually called in the init function of a module,
 * which provides payload hadlers.
 */
int 
kedr_coi_payload_register(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload* payload);


/*
 * Unregister payload, the interceptor will not use it any more.
 * 'payload' should be the same as passed to the corresponding call to
 * operations_interceptor_payload_register().
 * 
 * Returns 0 if successful, an error code otherwise.
 *
 * This function is usually called in the cleanup function of a module,
 * which provides payload handlers.
 */
int 
kedr_coi_payload_unregister(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload* payload);


/*
 * Interceptor goes into interception state: all currently registered
 * payloads become used, and list of payloads become fixed, that is
 * payloads registration/deregistration always return error(-EBUSY).
 * 
 * In interception state interceptor can watch for objects, and trigger
 * handlers for operations on that objects.
 * (See kedr_coi_interceptor_watch().)
 */
int kedr_coi_interceptor_start(struct kedr_coi_interceptor* interceptor);

/*
 * Interceptor leaves interception state: payload become unused and
 * payloads registering/deregistering is allowed.
 * 
 * All objects which are watched at this stage will be forgotten
 * using mechanism similar to kedr_coi_interceptor_forget_norestore.
 * Usually existance of such objects is a bug in objects' lifetime
 * determination mechanism (objects are already destroyed but interceptor
 * wasn't notified about that with one of 'kedr_coi_forget*' methods).
 * 
 * If 'trace_unforgotten_object' callback is set for interceptor,
 * it will be called for each unforgotten object.
 */
void kedr_coi_interceptor_stop(struct kedr_coi_interceptor* interceptor);

/*
 * Mark object as 'watched', callback operations for this object will
 * be intercepted and pre- and post-handlers in the registered payloads
 * will be called for them.
 * 
 * Object should have type for which interceptor is created.
 * 
 * This operation somewhat change content of object's fields concerned
 * with operations. If these fields are changed outside of the interceptor,
 * this function should be called again.
 * 
 * NOTE: This operation should be called only in 'interception' state
 * of the interceptor. Otherwise it will return error(-EINVAL).
 */
int kedr_coi_interceptor_watch(
    struct kedr_coi_interceptor* interceptor,
    void* object);

/*
 * Stop to watch for the object, from that moment callback operations for
 * that object will not be intercepted.
 * 
 * Also restore content of the object's fields concerned operations as 
 * it was before watching.
 * 
 * Return 0 on success, negative error code on fail.
 * If object wasn't watched, function return 1.
 *
 * NOTE: This operation should be called only in 'interception' state
 * of the interceptor. Otherwise it will return error(-EINVAL).
 */
int kedr_coi_interceptor_forget(
    struct kedr_coi_interceptor* interceptor,
    void* object);

/*
 * Forget interception information about object which was wached.
 * 
 * As opposed to kedr_operations_interceptor_forget_object(),
 * this function does not restore objects operations and does not access
 * to any fields of that object. So, this function may be used for
 * objects which may be already freed.
 * 
 * Return 0 on success, negative error code on fail.
 * If object wasn't watched, function return 1.
 *
 * NOTE: This operation should be called only in 'interception' state
 * of the interceptor. Otherwise it will return error(-EINVAL).
 */

int kedr_coi_interceptor_forget_norestore(
    struct kedr_coi_interceptor* interceptor,
    void* object);

/**********Creation of the operations interceptor*******************/

/*
 * Information for intermediate operation.
 */
struct kedr_coi_intermediate_info
{
    // Chained operation, which should be called.
    void* op_chained;
    // Original operation, which should be reported to the handlers.
    void* op_orig;
    // NULL-terminated array of functions of pre handlers for this operation.
    void* const* pre;
    // NULL-terminated array of functions of post handlers for this operation.
    void* const* post;
};


/*
 * Get information about intermediate for given operation in the given object.
 * 
 * After successfull call of this function 'info' structure will be filled.
 * 
 * This function is intended to be used ONLY in the implementation
 * of the intermediate operation.
 * 
 * Return 0 on success or negative error code on fail.
 * 
 * NOTE: Fail usually means unrecoverable bug.
 */

int kedr_coi_interceptor_get_intermediate_info(
    struct kedr_coi_interceptor* interceptor,
    const void* object,
    size_t operation_offset,
    struct kedr_coi_intermediate_info* info);


/*
 * Replacement for operation which should call registered pre- and
 * post-handlers and chained(original) operation in correct order.
 * 
 * NOTE: For normal interceptors 'op_orig' and 'op_chained' fields of
 * 'kedr_coi_intermediate_info' structure are always the same.
 * For others types of interceptor it is not true - see below.
 */
struct kedr_coi_intermediate
{
    size_t operation_offset;
    void* repl;
    
    /* 
     * If not 0, this is a group identifier of the intermediate function.
     * 
     * All intermediate functions used for particular interceptor
     * which share group identifier either are used together or aren't
     * used at all.
     * 
     * The thing is that default behaviour of some operations cannot be
     * implemented exactly as in the kernel(not all kernel functions are
     * available for modules).
     * 
     * Example of such operation is alloc_inode() operations in
     * 'struct super_operations': default behaviour in the kernel is 
     * allocate inode in some kmem_cache, but this cache is not available
     * for kernel module.
     * For correctness it is enough to implement this operation
     * consistently with destroy_inode(), which should destroy inode
     * allocated by alloc_inode(), so both these operations may use
     * shared kmem_cache, which is differ from kernel implementation's one.
     * 
     * But if one would like to intercept alloc_inode() without
     * interception of destroy_inode(), default behaviour of KEDR COI
     * is to use intermediate operation for alloc_inode() but do not use
     * intermediate operation for destroy_inode(), which lead to incon-
     * systency between this two operations: kernel implementation of
     * default destory_inode() operation unable to destroy inode, created
     * by default alloc_inode() in intermediate implementation.
     * 
     * Grouping force using of intermediate for destroy_inode() even
     * without interception of this operation.
     */
    int group_id;
    /*
     *  If flag is set, intermediate is not applicable for external
     * interception.
     * 
     * In that case, external handlers may not be used for this
     * operation and pointer to original operation is always not NULL
     * inside this intermediate.
     */
    bool internal_only;
};

/*
 * Create interceptor for specific type of object with operations.
 * 
 * @name is a name of the interceptor.
 * @operations_field_offset is an offset of the pointer
 *      to the operations struct inside object structure.
 * @operations_struct_size is a size of operations struct.
 * @intermediate_operations is an array of all known operations
 *      in the operations struct. Last element in that array should have
 *      '-1' in @operation_offset field.
 * 
 * Returns interceptor descriptor.
 */

struct kedr_coi_interceptor*
kedr_coi_interceptor_create(const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_intermediate* intermediate_operations);


/*
 * Create interceptor for specific type of object with operations.
 * 
 * This function is intended to use for objects, which has direct
 * pointers to operations instead of pointer to the operations structure.
 * 
 * @name is a name of the interceptor.
 * @object_size is a size of object struct.
 * @intermediate_operations contain array of all known operations
 *      in the object struct. Last element in that array should have '-1'
 *      in 'operation_offset' field.
 * 
 * Returns interceptor descriptor.
 */

struct kedr_coi_interceptor*
kedr_coi_interceptor_create_direct(const char* name,
    size_t object_size,
    const struct kedr_coi_intermediate* intermediate_operations);


void
kedr_coi_interceptor_destroy(
    struct kedr_coi_interceptor* interceptor);

/**Interceptor for object creation using its operations in factory object**/

/*
 * This type of interceptor is intended to use for objects with
 * operations which are not executed at place, but are copied into
 * another object(s) and are executed for it.
 * Initial object is used as a factory for another object in that case.
 * 
 * Unlike from normal interceptor, which is used for intercepts object's
 * callback operations, this one intercepts creation of another object.
 * If such object was created from the factory object, which is watched
 * by this interceptor, then newly created object will be automatically
 * watched by the normal interceptor.
 * 
 * '*_watch'(), '*_forget'(), '*_forget_norestore'() functions for
 * factory interceptor have behavour similar to ones for
 * interceptor of normal operations.
 * 
 * '_register_payload()' and '_unregister_payload' functions also similar
 * to normal interceptor's one, but registered handlers are called
 * only when normal object is created using factory operations.
 * 
 * Normally, payloads for that type of interceptor are used only for
 * define actions which depend on object creation way. E.g., 'struct file'
 * object can be created from inode, character device or common device.
 * Creation-way-independed actions may be defined via payloads for normal
 * interceptor.
 * 
 * Factory interceptor starts automatically when corresponded normal
 * interceptor is started, similar for stop.
 */

struct kedr_coi_factory_interceptor;

int kedr_coi_factory_interceptor_watch(
    struct kedr_coi_factory_interceptor* interceptor,
    void* factory);

int kedr_coi_factory_interceptor_forget(
    struct kedr_coi_factory_interceptor* interceptor,
    void* factory);

int kedr_coi_factory_interceptor_forget_norestore(
    struct kedr_coi_factory_interceptor* interceptor,
    void* factory);

void kedr_coi_factory_interceptor_destroy(
    struct kedr_coi_factory_interceptor* interceptor);
    
int kedr_coi_factory_payload_register(
    struct kedr_coi_factory_interceptor* interceptor,
    struct kedr_coi_payload* payload);

int kedr_coi_factory_payload_unregister(
    struct kedr_coi_factory_interceptor* interceptor,
    struct kedr_coi_payload* payload);


/*******Creation of the foreign operations interceptor****************/

/*
 * Information for intermediate operation.
 * 
 * NOTE: Unlike to normal interceptor, 'op_orig' and 'op_chained' fields
 * of 'kedr_coi_intermediate_info' structure are differs, when use with
 * factory interceptor.
 */
struct kedr_coi_intermediate_info;


/*
 * If given factory object is watched by factory interceptor, watch for
 * the given object.
 * 
 * Return negative error code on fail. In other cases, 0 is returned.
 * 
 * This function is intended to be used ONLY in the implementation
 * of the intermediate operation for factory interceptor.
 * 
 * 'op_chained' will be set to operation which should be called at the end
 * of intermediate operation. If fail to calculate chained operation,
 * 'op_chained' will be set to ERR_PTR().
 * 
 * If 0 is returned then 'op_chained' is a correct operation
 * (not an error indicator).
 */

int kedr_coi_factory_interceptor_bind_object(
    struct kedr_coi_factory_interceptor* interceptor,
    void* object,
    const void* factory,
    size_t operation_offset,
    struct kedr_coi_intermediate_info* info);

/*
 * Replacement for operation which should call
 * kedr_coi_factory_interceptor_bind_object() and then call chained
 * operation which is set by that function.
 * If there are handlers for the operation, them should be called before
 * and after chained operation.
 * 
 * NOTE: When describe intermediate operation for factory interceptor,
 * 'is_internal' flag in 'kedr_coi_intermediate' structure shouldn't be
 * set: replacement will always be called, whenever original operation
 * is NULL or not.
 * Moreover, replacement will be called even when no handlers are
 * registered for that operation. So, setting 'group_id' field has
 * no sence.
 */
struct kedr_coi_intermediate;

/*
 * Create interceptor for specific type of factory object.
 * 
 * 'interceptor_indirect' - interceptor which is used for the operations
 * with its natural object.
 *
 * 'name' is a name of the interceptor created and used internally(in messages).
 * 
 * 'factory_operations_field_offset' is an offset of the pointer
 *      to the operations struct inside factory object structure.
 * 'intermediate_operations' contain array of all known operations
 *      in the operations struct, which are called when object
 *      is created. Last element in that array should have '-1'
 *      in 'operation_offset' field.
 *      Usually this array contains only one operation.
 * 
 * Function returns interceptor descriptor.
 * 
 */

struct kedr_coi_factory_interceptor*
kedr_coi_factory_interceptor_create(
    struct kedr_coi_interceptor* interceptor,
    const char* name,
    size_t factory_operations_field_offset,
    const struct kedr_coi_intermediate* intermediate_operations);

/*************** Generic factory interceptor **************************/

/*
 * Generalization of factory interceptor.
 * 
 * Normally factory interceptor uses same 'factory' pointer for 
 * refer to particular watch when forget it or bind to it, and deduce
 * operations structure for intercept from that pointer.
 * 
 * In general, all these 3 actions may use distinct pointers.
 * Literally:
 * 
 * - id
 *      for identify watch when forget it,
 * - tie
 *      for identify watch when bind to it,
 * - ops_p
 *      for refer to the pointer to operations structure.
 * 
 * Here 'id' should be unique between different watches
 * (otherwise 'update watch' will be performed).
 * 
 * 'ops_p' should also be unique, otherwise attempt to watch for an
 *  operations returns error.
 * 
 * But 'tie' doesn't required to be unique. When perform binding to 'tie'
 * shared by different watches, any one of them may be used.
 */

/*
 * Generalization of kedr_coi_factory_interceptor_watch().
 * 
 * Watch for operations at 'ops_p'.
 * 
 * 'id' identificates given watch when performs
 * kedr_coi_factory_interceptor_forget_generic().
 * 
 * 'tie' used for find the watch when performs bind.
 * Distinct watches may share same 'tie'.
 */
int kedr_coi_factory_interceptor_watch_generic(
    struct kedr_coi_factory_interceptor* interceptor,
    void* id,
    void* tie,
    const void** ops_p);


/* 
 * Generalization of kedr_coi_factory_interceptor_forget().
 * 
 * Forget watch identified by 'id'.
 * 
 * If 'ops_p' is not NULL, restore operations.
 * 
 * Otherwise doesn't use this pointer; that case corresponds to
 * kedr_coi_factory_interceptor_forget_norestore().
 */
int kedr_coi_factory_interceptor_forget_generic(
    struct kedr_coi_factory_interceptor* interceptor,
    void* id,
    const void** ops_p);

/*
 * Variant of kedr_coi_factory_interceptor_bind_object() for use with
 * generic factory interceptor.
 * 
 * The only difference between functions is that this variant use 'tie'
 * instead of 'factory' for identify factory watch.
 */
static inline int kedr_coi_factory_interceptor_bind_object_generic(
    struct kedr_coi_factory_interceptor* interceptor,
    void* object,
    void* tie,
    size_t operation_offset,
    struct kedr_coi_intermediate_info* info)
{
    return kedr_coi_factory_interceptor_bind_object(interceptor,
        object, tie, operation_offset, info);
}

/* 
 * Create generic factory interceptor.
 * 
 * Similar to kedr_coi_factory_interceptor_create(), except:
 * 
 * 1) Intermediate operaions, described in 'intermediate_operations'
 * should call kedr_coi_factory_interceptor_bind_object_generic() instead of
 * kedr_coi_factory_interceptor_bind_object()
 * 2) 'trace_unforgotten_ops' callback accept two identificators of
 * watch: 'id' and 'tie'.
 * 
 * Functions, applicable to generic factory interceptor:
 * 
 * - kedr_coi_factory_interceptor_watch_generic()
 * - kedr_coi_factory_interceptor_forget_generic()
 * - kedr_coi_factory_payload_register()
 * - kedr_coi_factory_payload_unregister()
 * - kedr_coi_factory_interceptor_bind_object_generic()
 * - kedr_coi_factory_interceptor_destroy()
 * 
 * It is an error to use other functions with that interceptor.
 */
struct kedr_coi_factory_interceptor*
kedr_coi_factory_interceptor_create_generic(
    struct kedr_coi_interceptor* interceptor_indirect,
    const char* name,
    const struct kedr_coi_intermediate* intermediate_operations);

/*********** Methods which affects on interceptor's behaviour**********/
/* 
 * Set function to be called for each object which will be watched
 * when kedr_coi_interceptor_stop() is called.
 * 
 * Note, that object's structure may be freed at that moment,
 * so callback shouldn't access its fields.
 * 
 * Usually this callback is used for debugging purposes.
 * 
 * NULL means no callback, and it is default.
 */
void kedr_coi_interceptor_trace_unforgotten_object(
    struct kedr_coi_interceptor* interceptor,
    void (*cb)(const void* object));

/*
 * Similar to kedr_coi_interceptor_trace_unforgotten_object(),
 * but uses for factory interceptor.
 * 
 * When applied to interceptor create by
 * kedr_coi_factory_interceptor_create_generic(),
 * 'object' corresponds to 'id' of the watch.
 */
void kedr_coi_factory_interceptor_trace_unforgotten_object(
    struct kedr_coi_factory_interceptor* interceptor,
    void (*cb)(const void* object));


/* 
 * Return true, if given addr is inside some module.
 * Intended to be called with preemption disabled.
 * 
 * This is a default selector of instrumentation mechanism
 * (see kedr_coi_interceptor_mechanism_selector() description below).
 */
bool kedr_coi_is_module_address(const void* addr);

/*
 * Change selector for operations interception mechanism.
 * 
 * If selector returns true, operations themselves will be replaced
 * for implement interception semantic. This mechanism is referred
 * as 'at_place' below.
 * 
 * Otherwise, original operations structure is treated as read-only,
 * and newly created operations structure is used for implement
 * interception semantic, by replacing original operations pointer by
 * new one. This mechanism is referred as 'use_copy' below.
 * 
 * 'at_place' interception mechanism is more stable, because
 * 'use_copy' interception is lost when driver's code repeat assignment
 * to the pointer to operations structure. For make interception work
 * in that case, one should call kedr_coi_interceptor_watch() again.
 * 
 * Note, that selector's callback is called under lock taken.
 * When pointer to operations struct is NULL, special interception
 * mechanism is used, selector callback is not called in that case.
 * 
 * Selector can be set only for interceptor, created by
 * kedr_coi_interceptor_create(). 'direct' interceptor always use
 * 'at_place' mechanism, as there is no pointer to operations stored in
 * object. Factory interceptor use selector from normal interceptor it
 * binded to.
 * 
 * Default selector is &kedr_coi_is_module_address, and it is sufficient
 * in the most cases.
 */
void kedr_coi_interceptor_mechanism_selector(
    struct kedr_coi_interceptor* interceptor,
    bool (*replace_at_place)(const void* ops));

#endif /* OPERATIONS_INTERCEPTION_H */
