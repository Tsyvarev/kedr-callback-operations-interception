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
 * which operations you want to intercept and how.
 * 
 * 3. Register payload for operations interceptor created at step 2.
 * 
 * 4. Then you need to determine when object interested your is created,
 * and call kedr_coi_interceptor_watch() at this moment.
 * Depended from how objects are created, there are several ways to do this:
 * 
 * a) some objects are created by top-level kernel functions, such as 
 * cdev_add() or register_filesystem(). You may use KEDR for intercept
 * these functions and call kedr_coi_interceptor_watch()
 * in pre- or post- handlers of them.
 * 
 * b) some objects are created in operations of another objects. E.g.
 * super blocks (struct super_block) are created in mount() operation
 * of file system type objects (struct file_system_type). So you need to
 * intercept operations of creator's objects, for which you should repeat
 * all steps from 1 of this description (perhaps, recursively).
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
 * But it is known, that operations for file object are copied from inode
 * (struct inode) object when file is created. You can intercept creation
 * of file if you use interceptor for file operations in inode object.
 * Such interceptor is referred as foreign, because it allows to intercept
 * operations which are called not for the owner, but for another object.
 * Interceptors for foreign operations doesn't allow to set handlers for these
 * operations but allow to set handler to be executed when foreign object is created.
 * Creation of interceptors for foreign operations are described in
 * section "Foreign interceptor creation".
 * 
 * 5. Determine when objects interested your are destroyed, and call
 * kedr_coi_interceptor_forget() at that moment.
 * As in step 4, ways for doing depends from the way haw objects are destroyed.
 * Cases a) and b) from step 4 are applicable for interception of
 * object destruction wihtout changes.
 * As for objects which are created internally, them are usually destroyed after
 * some of its operation. E.g., files objects are destroyed after their
 * release() method. So setting handler for this operation is sufficient
 * for intercept object destruction.
 * 
 * 6. Before the first object may be watched by the interceptor, one should
 * call kedr_coi_interceptor_start() for this interceptor.
 * This call will fix(prevent from changes) interception handlers
 * of this interceptor, and create some additional data used for interception.
 * Usually this function is called from corresponded callback of KEDR payload.
 * 
 * Similar, after last object is forgot one should call
 * kedr_coi_interceptor_stop() for allow to unload payload modules or
 * load another ones.
 * Usually this function is called from corresponded callback of KEDR payload.
 * 
 * 
 * 
 * Interceptor creation.
 * 
 * For create interceptor for operations of object of some type, one should:
 * 
 * a) describe "geometry" of the operations inside the object.
 * 
 * For objects which have pointer to struct with all operations
 * as a field it is needed to set offset of this field in the object struct
 * and size of operations struct.
 * E.g., for interceptor of file operations this values are
 * offsetof(struct file, f_op) and sizeof(struct file_operations).
 * 
 * For objects which have pointers to every operation as a distinct fields
 * it is needed to set size of object struct itself.
 * E.g., for interceptor of file system type operations it is
 * sizeof(struct file_system_type).
 * 
 * b) write intermediate operations for all object operations which
 * is planned to intercept.
 * 
 * Each such operation should firstly call all pre- handlers, registered
 * for this operation, then original operation and then all post- handlers.
 * 
 * Arrays of pre-handlers and post-handlers are static and filled by
 * interceptor in kedr_coi_interceptor_start()
 * call. Original operation is available via
 * kedr_coi_interceptor_get_orig_operation().
 * 
 * Intermediate operations should correctly process the case when
 * pointer to the original operation is NULL. Usuall in that case
 * some default actions should be performed,
 * depended of concrete intercepted operation.
 * E.g., default behaviour of open() file operation is simple returning 0.
 * It is the behaviour which is emulated by filesystem when pointer
 * to open() file operation is NULL.
 * Another example - alloc_inode() operation of super block, which default
 * behaviour is simple allocation of memory for inode structure.
 * 
 * Interceptors are created with functions
 * kedr_coi_interceptor_create() and
 * kedr_coi_interceptor_create_direct().
 * Former function creates interceptor for objects which have field pointed
 * to structure of operations, the latter process objects which have
 * distinct fields for different operations.
 * 
 * 
 * Foreign interceptor creation.
 * 
 * Interceptor for foreign operations is created very similar to one
 * for normal operations, organized into one structure
 * (see section "Interceptor creation.")
 * 
 * a) One should describe "geometry" of such operations in term of:
 * -offset of the field pointed to operations struct inside object struct
 * -size of operations struct
 * -offset of the field pointer to operations struct inside struct of foreign
 * object, for which these operations are called.
 * 
 * b) write intermediate operations for operations which are called just
 * after foreign object is created and its operations are copied from
 * initial object.
 * Normally it is one operation, which should firstly call
 * kedr_coi_interceptor_foreign_restore_copy() and pass to it 
 * pointer to the foreign object which is just created. This function
 * restore operations of the foreign object, which are copied from
 * changed operations of the initial object.
 * Intermediate operation then should call all handlers, registered for
 * interceptor. These handlers are organized into static array which is
 * filled inside kedr_coi_interceptor_start().
 * Finally, this operation should call original operation, pointer to
 * which is contained in operations structure, set for foreign object.
 * 
 * Foreign interceptor is created with kedr_coi_interceptor_create_foreign()
 * function.
 * 
 */

#ifndef OPERATIONS_INTERCEPTION_H
#define OPERATIONS_INTERCEPTION_H

#include <linux/module.h>

/*
 * Operations interceptor.
 * 
 * It is responsible for intercept operations for objects of
 * particular types, making available to call user-defined functions
 * before or after these operations(pre- and post-handlers).
 */
struct kedr_coi_interceptor;

/*
 * Information about original operation call,
 * which is passed to the functions, which intended to call
 * before or after it.
 */
struct kedr_coi_operation_call_info
{
    void* return_address;
};


/*
 * Pair of operation which should be intercepted and function,
 * which should be called before it.
 * 
 * If original operation has signature
 * ret_type (*)(arg_type1,..., arg_typeN)
 * 
 * then function should have signature
 * 
 * void (*)(arg_type1,..., arg_typeN, kedr_operation_call_info*).
 */

struct kedr_coi_handler_pre
{
    size_t operation_offset;
    void* func;
};

/*
 * Pair of operation which should be intercepted and function,
 * which should be called after it.
 * 
 * If original operation has signature
 * ret_type (*)(arg_type1,..., arg_typeN)
 * 
 * and ret_type is not 'void', then function should have signature
 * 
 * void (*)(arg_type1,..., arg_typeN, ret_type, kedr_operation_call_info*).
 * 
 * If original operation has signature
 * void (*)(arg_type1,..., arg_typeN)
 * 
 * then function should have signature
 * 
 * void (*)(arg_type1,..., arg_typeN, kedr_operation_call_info*).
 */

struct kedr_coi_handler_post
{
    size_t operation_offset;
    void* func;
};


/*
 * Contain information about what object's operations
 * one want to intercept and how.
 */
struct kedr_coi_payload
{
    struct module* mod;
    
    struct kedr_coi_handler_pre* handlers_pre;
    struct kedr_coi_handler_post* handlers_post;
};


/* 
 * Registers a payload module with the operations interceptor. 
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
 *  Unregisters a payload module, the interceptor will not use it any more.
 * 'payload' should be the same as passed to the corresponding call to
 * operations_interceptor_payload_register().
 * 
 * This function is usually called in the cleanup function of a payload 
 * module.
 */
void 
kedr_coi_payload_unregister(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload* payload);


/*
 * After call of this function payload set for interceptor become fixed
 * and interceptor may be applied to the objects for make their operations
 * available for interception.
 */
int kedr_coi_interceptor_start(struct kedr_coi_interceptor* interceptor);

/*
 * After call of this function payload set for interceptor become
 * flexible again.
 */
void kedr_coi_interceptor_stop(struct kedr_coi_interceptor* interceptor);


/*
 * Watch for the operations of the particular object and intercepts them.
 * 
 * Object should have type for which interceptor is created.
 * 
 * This operation somewhat change content of object's fields concerning
 * operations. If these fields are changed outside of the replacer,
 * this function should be called again.
 * 
 */
int kedr_coi_interceptor_watch(
    struct kedr_coi_interceptor* interceptor,
    void* object);

/*
 * Stop to watch for the object and restore content of the
 * object's fields concerned operations.
 */
int kedr_coi_interceptor_forget(
    struct kedr_coi_interceptor* interceptor,
    void* object);

/*
 * Stop to watch for the object but do not restore content of the
 * object's operations field.
 * 
 * This function is intended to call instead of
 * kedr_operations_interceptor_forget_object()
 * when object may be already freed and access
 * to its operations field may cause memory fault.
 */
int kedr_coi_interceptor_forget_norestore(
    struct kedr_coi_interceptor* interceptor,
    void* object);

/**********Creation of the operations interceptor*******************/

/*
 * Information for intermediate operation
 */
struct kedr_coi_intermediate_info
{
    // NULL-terminated array of pre-functions
    void** pre;
    // NULL-terminated array of post-functions
    void** post;
};


/*
 * Return operation which is replaced by intermediate one.
 * 
 * This function is intended to be used ONLY in the implementation
 * of the intermediate operation.
 * 
 * Note: Function may return NULL, when operation is not defined
 * in the object initially. Intermediate operation should correctly
 * process this case.
 */

void* kedr_coi_interceptor_get_orig_operation(
    struct kedr_coi_interceptor* interceptor,
    const void* object,
    size_t operation_offset);


/*
 * Replacement for operation which call registered pre- and post-handlers
 * and original operation in correct order.
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
     * intermediate operation for detroy_inode(), which lead to incon-
     * systency between this two operations: kernel implementation of
     * default destory_inode() operation unable to destroy inode, created
     * by default destroy_inode() in intermediate implementation.
     * 
     * Grouping force using of intermediate for destroy_inode() even
     * without interception of this operation.
     */
    int group_id;
    
    struct kedr_coi_intermediate_info* info;
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
 * 
 * Function return interceptor descriptor.
 */

struct kedr_coi_interceptor*
kedr_coi_interceptor_create(const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_intermediate* intermediate_operations);


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
 * Function return interceptor descriptor.
 */

struct kedr_coi_interceptor*
kedr_coi_interceptor_create_direct(const char* name,
    size_t object_size,
    struct kedr_coi_intermediate* intermediate_operations);


void
kedr_coi_interceptor_destroy(
    struct kedr_coi_interceptor* interceptor);


/**************Interceptor for foreign operations*******************/

/*
 * This type of interceptor is intended to use for object's operations,
 * which are copied into another object('foreign object'),
 * and are called only for that object.
 * 
 * Key difference of this interceptor from 'normal' interceptors is that,
 * even it may intercept any operation, it permit to register only
 * one type of handlers, which will be called after 'foreign object'
 * is created but before any of its operation is called.
 * For set handlers for particular operations, one need to use 'normal'
 * interceptor for foreign object.
 */

// Type of handler functions for foreign interceptor.
typedef void (*kedr_coi_handler_foreign_t)(void* foreign_object);

/*
 * Information for intermediate foreign operation.
 * 
 * Unlike information for 'normal' intermediate operation, this information
 * is shared between all intermediate operations.
 * (But usually there is only one intermediate operation)
 */
struct kedr_coi_intermediate_foreign_info
{
    //NULL- terminated array of functions which should be called after
    // foreign object is created
    kedr_coi_handler_foreign_t* on_create_handlers;
};


/*
 * Return copied operations to their initial state.
 * 
 * This function is intended to be used ONLY in the implementation
 * of the intermediate foreign operation.
 */

int kedr_coi_interceptor_foreign_restore_copy(
    struct kedr_coi_interceptor* interceptor,
    void* object,
    void* foreign_object);


/*
 * Replacement for operation which restore operations for foreign object,
 * call registered handlers and then call initial operation.
 */
struct kedr_coi_intermediate_foreign
{
    size_t operation_offset;
    void* repl;
};

/*
 * Create interceptor for specific type of object with operations
 * for foreign object(not for object itself).
 * 
 * 'name' is a name of the interceptor and used internally(in messages)
 * 'operations_field_offset' is an offset of the pointer
 *      to the operations struct inside object structure.
 * 'operations_struct_size' is a size of operations struct.
 * 'foreign_operations_field_offset' is an offset of the pointer
 *      to the operations struct inside foreign object structure.
 * 'intermediate_operations' contain array of all known operations
 *      in the operations struct, which are called when foreign
 *      object is created. Last element in that array should have '-1'
 *      in 'operation_offset' field.
 *      Usually this array contains only one operation.
 * 'intermediate_info' is an information used by intermediate operation(s).
 * 
 * Function returns interceptor descriptor.
 */

struct kedr_coi_interceptor*
kedr_coi_interceptor_create_foreign(
    const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    size_t foreign_operations_field_offset,
    struct kedr_coi_intermediate_foreign* intermediate_operations,
    struct kedr_coi_intermediate_foreign_info* intermediate_info);

/*
 * Contain information about what operations one want to intercept
 * and how.
 */
struct kedr_coi_payload_foreign
{
    struct module* mod;
    /*
     * NULL-terminated array of functions, which are called when
     * foreign object is created.
     */
    kedr_coi_handler_foreign_t* on_create_handlers;
};

/* Registers a payload module with the foreign operations interceptor. 
 * 'payload' should provide all the data the interceptor needs to use this 
 * payload module.
 * This function returns 0 if successful, an error code otherwise.
 * 
 * This function is usually called in the init function of a payload module.
 * */
int 
kedr_coi_payload_foreign_register(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload_foreign *payload);

/*
 *  Unregisters a payload module, the interceptor will not use it any more.
 * 'payload' should be the same as passed to the corresponding call to
 * operations_interceptor_payload_register().
 * 
 * This function is usually called in the cleanup function of a payload 
 * module.
 * */
void 
kedr_coi_payload_foreign_unregister(
    struct kedr_coi_interceptor* interceptor,
    struct kedr_coi_payload_foreign *payload);

/**************************************/


#endif /* OPERATIONS_INTERCEPTION_H */