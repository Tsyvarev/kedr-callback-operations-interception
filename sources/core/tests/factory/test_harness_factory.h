/*
 * Infrastructure for test callback operation interception(factory functionality).
 */

#ifndef KEDR_COI_TEST_HARNESS_FACTORY
#define KEDR_COI_TEST_HARNESS_FACTORY

#include "test_harness.h"

static inline void factory_intermediate_func_common(void* object,
    void* data,
    void* (*get_factory)(void* data),
    struct kedr_coi_factory_interceptor* interceptor,
    size_t operation_offset)
{
    struct kedr_coi_factory_intermediate_info info;
    
    kedr_coi_bind_object_with_factory(interceptor,
        object,
        get_factory(data),
        operation_offset,
        &info); 

    //TODO: call handlers
    
    //pr_info("In factory intermediate operation 'op_chained' is %pF.", op_chained);
    if(info.op_chained != NULL)
    {
        void (*op_chained)(void* object, void* data) =
            (typeof(op_chained))info.op_chained;
        op_chained(object, data);
    }
}

#define KEDR_COI_TEST_DEFINE_FACTORY_INTERMEDIATE_FUNC(func_name, get_factory, operation_offset, interceptor)  \
void func_name(void* object, void* data) \
{ \
    factory_intermediate_func_common(object, data, get_factory, interceptor, operation_offset); \
}


#endif /* KEDR_COI_TEST_HARNESS_FACTORY */
