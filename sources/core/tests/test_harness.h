/*
 * Infrastructure for test callback operation interception
 */

#ifndef KEDR_COI_TEST_HARNESS
#define KEDR_COI_TEST_HARNESS

typedef void (*kedr_coi_test_op_t)(void* object, void* another_object);

#define INTERMEDIATE_FINAL {.operation_offset = -1}

#ifdef OPERATION_OFFSET

#define INTERMEDIATE(op_name, op_repl) {.operation_offset = OPERATION_OFFSET(op_name), .repl = (void*)&op_repl}

#define PRE_HANDLER(op_name, handler_func){.operation_offset = OPERATION_OFFSET(op_name), .func = (void*)&handler_func}

#define POST_HANDLER(op_name, handler_func){.operation_offset = OPERATION_OFFSET(op_name), .func = (void*)&handler_func}

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

    //pr_info("In intermediate original operation is %pF.", op_orig);
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


#define KEDR_COI_TEST_DEFINE_PRE_HANDLER_FUNC(func_name, counter) \
void func_name(void* object, void* data, struct kedr_coi_operation_call_info* info)\
{\
    counter++;\
}


#define KEDR_COI_TEST_DEFINE_POST_HANDLER_FUNC(func_name, counter) \
void func_name(void* object, void* data, struct kedr_coi_operation_call_info* info)\
{\
    counter++;\
}

#define KEDR_COI_TEST_DEFINE_OP_ORIG(func_name, counter) \
void func_name(void* object, void* data)\
{\
    counter++;\
}


#endif /*KEDR_COI_TEST_HARNESS*/
