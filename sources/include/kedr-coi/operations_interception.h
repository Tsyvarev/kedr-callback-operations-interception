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
 * Handler whoc should be executed after callback operation.
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
    struct module* mod;
    
    // Array of pre-handlers ended with mark
    struct kedr_coi_pre_handler* pre_handlers;
    // Array of post-handlers ended with mark
    struct kedr_coi_post_handler* post_handlers;
};


/* 
 * Register a payload module with the operations interceptor. 
 * 'payload' should provide all the data the interceptor needs to use this 
 * payload module.
 * This function returns 0 if successful, an error code otherwise.
 * 
 * This function is usually called in the init function of a payload module.
 */
int 
kedr_coi_payload_register(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload* payload);


/*
 *  Unregister a payload module, the interceptor will not use it any more.
 * 'payload' should be the same as passed to the corresponding call to
 * operations_interceptor_payload_register().
 * 
 * This function is usually called in the cleanup function of a payload 
 * module.
 */
int 
kedr_coi_payload_unregister(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload* payload);


/*
 * After call of this function payload set for interceptor become fixed
 * and interceptor goes into interception state.
 */
int kedr_coi_interceptor_start(struct kedr_coi_interceptor* interceptor);

/*
 * After call of this function interceptor leaves interception state
 * ans payload set for interceptor become flexible again.
 * 
 * All objects which are watched at this stage will be forgotten
 * using mechanism similar to kedr_coi_interceptor_forget_norestore.
 * Usually existance of such objects is a bug in objects' lifetime
 * determination mechanism (objects are alredy destroyed but interceptor
 * wasn't notified about that with '_forget' methods).
 * If 'trace_unforgotten_object' passed to the interceptor constructor
 * is not NULL it will be called for each unforgotten object.
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
 * with operations. If these fields are changed outside of the replacer,
 * this function should be called again.
 * 
 * NOTE: This operation should be called only in 'interception' state
 * of the interceptor. Otherwise it will return error.
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
 * of the interceptor. Otherwise it will return error.
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
 * of the interceptor. Otherwise it will return error.
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
    // Original operation
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
 * NOTE: fail usually means unrecoverable bug.
 */

int kedr_coi_interceptor_get_intermediate_info(
    struct kedr_coi_interceptor* interceptor,
    const void* object,
    size_t operation_offset,
    struct kedr_coi_intermediate_info* info);


/*
 * Replacement for operation which should call registered pre- and
 * post-handlers and original operation in correct order.
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
     * by default destroy_inode() in intermediate implementation.
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
 * 'name' is name of the interceptor and used internally(in messages)
 * 'operations_field_offset' is an offset of the pointer
 *      to the operations struct inside object structure.
 * 'operations_struct_size' is a size of operations struct.
 * 'intermediate_operations' contain array of all known operations
 *      in the operations struct. Last element in that array should have '-1'
 *      in 'operation_offset' field.
 * If not NULL, 'trace_unforgotten_object' will be called from 
 * kedr_coi_interceptor_stop() for each object which will be watched
 * at that moment.
 * 
 * Function return interceptor descriptor.
 */

struct kedr_coi_interceptor*
kedr_coi_interceptor_create(const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object));


/*
 * Create interceptor for specific type of object with operations.
 * 
 * This function is intended to use for objects, which has direct
 * pointers to operations instead of pointer to the operations structure.
 * 
 * 'name' is name of the interceptor and used internally(in messages)
 * 'object_size' is a size of object struct.
 * 'intermediate_operations' contain array of all known operations
 *      in the object struct. Last element in that array should have '-1'
 *      in 'operation_offset' field.
 * 
 * If not NULL, 'trace_unforgotten_object' will be called from 
 * kedr_coi_interceptor_stop() for each object which will be watched
 * at that moment.
 * Function return interceptor descriptor.
 */

struct kedr_coi_interceptor*
kedr_coi_interceptor_create_direct(const char* name,
    size_t object_size,
    struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object));


void
kedr_coi_interceptor_destroy(
    struct kedr_coi_interceptor* interceptor);


/*
 * Create interceptor similar to one returned by 
 * kedr_coi_interceptor_create(), but do not replace operations at place.
 * 
 * Instead, copy operations struct, replace operations in it and
 * set pointer to operations inside object to copied operations struct.
 * 
 * Useful when original operation struct is write-protected or is widely
 * used by the system, so replacement its content is dangerous.
 * 
 * Should be used only when it is really needed, because this interception
 * mechanizm is disabled not only when ponter to operations is externally
 * set to other operations, but also when it set to original operations.
 */
struct kedr_coi_interceptor*
kedr_coi_interceptor_create_use_copy(const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object));

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
 * to normal interceptor's one, but may be used only for operation which
 * create normal object.
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
 * Unlike to intermediate operation for normal interceptor, which
 * call original operation, one for factory interceptor should distiniguish
 * original operation from operation which it should call.
 */
struct kedr_coi_factory_intermediate_info
{
    // Chained operation, which should be called.
    void* op_chained;
    // Original operation, which is reported to the handlers.
    void* op_orig;
    // NULL-terminated array of functions of pre handlers for this operation.
    void* const* pre;
    // NULL-terminated array of functions of post handlers for this operation.
    void* const* post;
};


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

int kedr_coi_bind_object_with_factory(
    struct kedr_coi_factory_interceptor* interceptor,
    void* object,
    void* factory,
    size_t operation_offset,
    struct kedr_coi_factory_intermediate_info* info);

/*
 * Replacement for operation which should call
 * kedr_coi_bind_object_with_factory() and then call chained operation.
 * which is set by that function. If there are handlers for the operation,
 * them should be called before and after chained operation.
 */
struct kedr_coi_factory_intermediate
{
    size_t operation_offset;
    void* repl;
    /*
     *  Intermediate for factory interceptor is always external -
     * it intercepts state-change. Moreover, it is called even when
     * no handlers for operation.
     */
};

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
 */

struct kedr_coi_factory_interceptor*
kedr_coi_factory_interceptor_create(
    struct kedr_coi_interceptor* interceptor_indirect,
    const char* name,
    size_t factory_operations_field_offset,
    const struct kedr_coi_factory_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object));

/*****************Interceptor object creation**************************/

/*
 * Generalization of factory interceptor.
 * 
 * When factory interceptor use same pointer('factory')
 * as identificator for registration and deregistration watching,
 * as identificator for check whether factory is watched in
 * kedr_coi_factory_bind() function and deduce operations pointer from it,
 * 
 * object creation interceptor distinguish all these cases. Literally:
 * 
 * 1) 'id' is used as identificator for registration and deregistration
 * watching,
 * 2) 'tie' is used for check whether 
 * 
 * This type of interceptor is intended to use for object's operations
 * which will be copied into another object(s) and are executed for it.
 * 
 * Unlike from normal interceptor, which is used for intercepts object's
 * callback operations for execute any actions, this interceptor
 * intercepts moment of creation of another object. If such object was
 * created and use given operations, then it will be automatically
 * watched by the normal interceptor.
 */

struct kedr_coi_creation_interceptor;

/*
 *  Accept by reference pointer to operations structure.
 * Change that pointer or operations pointer by it so, that operations
 * retain semantic that them had before call plus:
 *  - any newly-created object which use these operations will be
 *    watched by the binded(normal) interceptor.
 * 
 * 'id' is an identificator which should be used for cancel such
 * objects' 'autowatching'.
 * 
 * 'tie' is something that describe originating of operations.
 * For newly-created object being watched, it should pass this pointer
 * to the 'autowatching' function (TODO: more precise description is required).
 * 
 * Return 0 on success and negative error code on fail.
 * 
 * If 'id' is already used for some operations replacement, update this
 * replacement and return 1 on success and negative error code on fail.
 * 
 * Comparision with factory interceptor:
 * kedr_coi_factory_interceptor_watch(*,factory)
 *      corresponds to
 * kedr_coi_creation_interceptor_watch(*, factory,
 *                          factory_ops_p, factory)
 * 
 * NOTE: Main difference from factory interceptor is that 'tie' may be
 * not unique for different factories.
 */
int kedr_coi_creation_interceptor_watch(
    struct kedr_coi_creation_interceptor* interceptor,
    void* id,
    void* tie,
    const void** ops_p);

/*
 * Accept identificator which was previously used for call
 * kedr_coi_creation_interceptor_watch().
 * 
 * Cancel objects' 'autowatching' registered for this id.
 * Also free all resources concerned with such 'autowatching'.
 * 
 * If 'ops_p' is not NULL, set it to the pointer to the original
 * operations, that was before watching.
 * 
 * Return 0 on success, negative error code on fail.
 * If 'id' isn't registered, return 1.
 * 
 * Comparision with factory interceptor:
 * kedr_coi_factory_interceptor_forget(*, factory)
 *      corresponds to
 * kedr_coi_creation_interceptor_forget(*, factory,
 *                                  factory_ops_p)
 * 
 * kedr_coi_factory_interceptor_forget_norestore(*, factory)
 *      corresponds to
 * kedr_coi_creation_interceptor_forget(*, factory, NULL)
 */

int kedr_coi_creation_interceptor_forget(
    struct kedr_coi_creation_interceptor* interceptor,
    void* id,
    const void** ops_p);

void kedr_coi_creation_interceptor_destroy(
    struct kedr_coi_creation_interceptor* interceptor);
    
/**********Creation of the interceptor for object's creation***********/

/*
 * Information for intermediate operation.
 * 
 * Unlike to intermediate operation for normal interceptor, which
 * call original operation, one for object creation's interceptor should
 * distiniguish original operation from operation which it should call.
 */
struct kedr_coi_creation_intermediate_info
{
    // Chained operation, which should be called.
    void* op_chained;
    // Original operation, which is reported to the handlers.
    void* op_orig;
    // NULL-terminated array of functions of pre handlers for this operation.
    void* const* pre;
    // NULL-terminated array of functions of post handlers for this operation.
    void* const* post;
};


/*
 * If 'tie' was used for some operations replacement (TODO: more presize
 * description is required), watch for a given 'object' with binded
 * interceptor.
 * 
 * This function is intended to be used ONLY in the implementation
 * of the intermediate for creation interceptor.
 * 
 * 'op_chained' will be set to operation which should be called at the end
 * of intermediate operation. If fail to calculate chained operation,
 * 'op_chained' will be set to ERR_PTR().
 * 
 * If 0 is returned then 'op_chained' is a correct operation
 * (not an error indicator).
 * 
 * Comparision with factory interceptor:
 * kedr_coi_bind_object_with_factory(*, object, factory,
 *                                      operation_offset, info)
 *    corresponds to
 * kedr_coi_bind_object(*, object, factory,
 *                  operation_offset, info)
 */

int kedr_coi_bind_object(
    struct kedr_coi_creation_interceptor* interceptor,
    void* object,
    void* tie,
    size_t operation_offset,
    struct kedr_coi_creation_intermediate_info* info);

/*
 * Replacement for operation which should call
 * kedr_coi_bind_object() and then call chained operation.
 * which is set by that function.
 */
struct kedr_coi_creation_intermediate
{
    size_t operation_offset;
    void* repl;
};

/*
 * Create interceptor for specific ...(TODO) type of object with operations
 * for foreign object(not for object itself).
 * 
 * 'interceptor_indirect' - interceptor which is used for the operations
 * with its natural object.
 *
 * 'name' is a name of the interceptor created and used internally(in messages).
 * 
 * 'intermediate_operations' contain array of all known operations
 *      in the operations struct, which are called when object
 *      is created. Last element in that array should have '-1'
 *      in 'operation_offset' field.
 *      Usually this array contains only one operation.
 * 
 * Function returns interceptor descriptor.
 */

struct kedr_coi_creation_interceptor*
kedr_coi_creation_interceptor_create(
    struct kedr_coi_interceptor* interceptor_indirect,
    const char* name,
    const struct kedr_coi_creation_intermediate* intermediate_operations,
    void (*trace_unforgotten_ops)(void* id, void* tie));

/**************************************/

#endif /* OPERATIONS_INTERCEPTION_H */