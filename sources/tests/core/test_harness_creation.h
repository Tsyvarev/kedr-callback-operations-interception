/*
 * Infrastructure for test callback operation interception(creation functionality).
 */

#ifndef KEDR_COI_TEST_HARNESS_CREATION
#define KEDR_COI_TEST_HARNESS_CREATION

#include "test_harness.h"

static inline void creation_intermediate_func_common(void* object,
    void* data,
    void* (*get_tie)(void* data),
    struct kedr_coi_factory_interceptor* interceptor,
    size_t operation_offset)
{
    void (*op_chained)(void* object, void* data);
    struct kedr_coi_intermediate_info info;
    struct kedr_coi_operation_call_info call_info;

    
    int result = kedr_coi_factory_interceptor_bind_object_generic(
        interceptor,
        object,
        get_tie(data),
        operation_offset,
        &info); 

    BUG_ON(result < 0);

    call_info.return_address = __builtin_return_address(0);
    call_info.op_orig = info.op_orig;
    op_chained = (typeof(op_chained))info.op_chained;

    if(info.pre)
    {
        void (**pre_handler)(void* object, void* data, struct kedr_coi_operation_call_info* info);
        for(pre_handler = (typeof(pre_handler))info.pre; *pre_handler != NULL; pre_handler++)
        {
            (*pre_handler)(object, data, &call_info);
        }
    }

    //pr_info("In factory intermediate operation 'op_chained' is %pf.", op_chained);
    if(op_chained != NULL)
    {
        op_chained(object, data);
    }
    
    if(info.post)
    {
        void (**post_handler)(void* object, void* data, struct kedr_coi_operation_call_info* info);
        for(post_handler = (typeof(post_handler))info.post; *post_handler != NULL; post_handler++)
        {
            (*post_handler)(object, data, &call_info);
        }
    }
}

#define KEDR_COI_TEST_DEFINE_CREATION_INTERMEDIATE_FUNC(func_name, get_tie, operation_offset, interceptor)  \
void func_name(void* object, void* data) \
{ \
    creation_intermediate_func_common(object, data, get_tie, interceptor, operation_offset); \
}

#endif /* KEDR_COI_TEST_HARNESS_CREATION */
