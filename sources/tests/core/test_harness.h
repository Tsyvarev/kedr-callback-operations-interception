/*
 * Infrastructure for test callback operation interception
 */

#ifndef KEDR_COI_TEST_HARNESS
#define KEDR_COI_TEST_HARNESS

typedef void (*kedr_coi_test_op_t)(void* object, void* another_object);

#define INTERMEDIATE_FINAL {.operation_offset = -1}

#ifdef OPERATION_OFFSET

#define INTERMEDIATE(op_name, op_repl) {\
    .operation_offset = OPERATION_OFFSET(op_name),\
    .repl = (void*)&op_repl\
}

#define INTERMEDIATE_INTERNAL_ONLY(op_name, op_repl) {\
    .operation_offset = OPERATION_OFFSET(op_name),\
    .repl = (void*)&op_repl,\
    .internal_only = true\
}

#define HANDLER_FUNC_CHECKED(handler_func, type) BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(&handler_func), type)) + (char*)&handler_func

typedef void (*kedr_coi_test_handler_t)(void*, void*, struct kedr_coi_operation_call_info*, int);
#define TEST_HANDLER_FUNC_CHECKED(handler_func) HANDLER_FUNC_CHECKED(handler_func, kedr_coi_test_handler_t)

#define HANDLER(op_name, handler_func){\
    .operation_offset = OPERATION_OFFSET(op_name),\
    .func = TEST_HANDLER_FUNC_CHECKED(handler_func)\
}

#define HANDLER_EXTERNAL(op_name, handler_func){\
    .operation_offset = OPERATION_OFFSET(op_name),\
    .func = TEST_HANDLER_FUNC_CHECKED(handler_func),\
    .external = true\
}

#endif /* OPERATION_OFFSET */

static inline void intermediate_func_common(void* object,
    void* data,
    struct kedr_coi_interceptor* interceptor,
    size_t operation_offset)
{
    void (*op_orig)(void* object, void* data);
    struct kedr_coi_operation_call_info call_info;
    struct kedr_coi_intermediate_info info; 
    
    kedr_coi_interceptor_get_intermediate_info(interceptor, object, operation_offset, &info); 

    call_info.return_address = __builtin_return_address(0);
    call_info.op_orig = info.op_orig;
    op_orig = (typeof(op_orig))info.op_orig;

    if(info.pre)
    {
        void (**pre_handler)(void* object, void* data, struct kedr_coi_operation_call_info* info);
        for(pre_handler = (typeof(pre_handler))info.pre; *pre_handler != NULL; pre_handler++)
        {
            (*pre_handler)(object, data, &call_info);
        }
    }

    //pr_info("In intermediate original operation is %pf.", op_orig);
    if(op_orig != NULL) op_orig(object, data);
    
    if(info.post)
    {
        void (**post_handler)(void* object, void* data, struct kedr_coi_operation_call_info* info);
        for(post_handler = (typeof(post_handler))info.post; *post_handler != NULL; post_handler++)
        {
            (*post_handler)(object, data, &call_info);
        }
    }

}

#define KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(func_name, operation_offset, interceptor)  \
void func_name(void* object, void* data) \
{ \
    intermediate_func_common(object, data, interceptor, operation_offset); \
}


#define KEDR_COI_TEST_DEFINE_HANDLER_FUNC(func_name, counter) \
void func_name(void* object, void* data, struct kedr_coi_operation_call_info* info, int unused)\
{\
    counter++;\
}

#define KEDR_COI_TEST_DEFINE_OP_ORIG(func_name, counter) \
void func_name(void* object, void* data)\
{\
    counter++;\
}

   
static inline bool replace_at_place_never(const void* ops)
{
    (void)ops;
    return 0;
}
static inline struct kedr_coi_interceptor* kedr_coi_interceptor_create_use_copy(
    const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    const struct kedr_coi_intermediate* intermediate_operations)
{
    struct kedr_coi_interceptor* interceptor = kedr_coi_interceptor_create(
        name, operations_field_offset, operations_struct_size,
        intermediate_operations);
    
    if(interceptor)
    {
        kedr_coi_interceptor_mechanism_selector(interceptor, &replace_at_place_never);
    }
    
    return interceptor;
}

static inline bool replace_at_place_always(const void* ops)
{
    (void)ops;
    return 1;
}
static inline struct kedr_coi_interceptor* kedr_coi_interceptor_create_at_place(
    const char* name,
    size_t operations_field_offset,
    size_t operations_struct_size,
    struct kedr_coi_intermediate* intermediate_operations)
{
    struct kedr_coi_interceptor* interceptor = kedr_coi_interceptor_create(
        name, operations_field_offset, operations_struct_size,
        intermediate_operations);
    
    if(interceptor)
    {
        kedr_coi_interceptor_mechanism_selector(interceptor, &replace_at_place_always);
    }
    
    return interceptor;
}


#endif /*KEDR_COI_TEST_HARNESS*/
