/*
 * Infrastructure for test callback operation interception(creation functionality).
 */

#ifndef KEDR_COI_TEST_HARNESS_CREATION
#define KEDR_COI_TEST_HARNESS_CREATION

#include "test_harness.h"

static inline void creation_intermediate_func_common(void* object,
    void* data,
    void* (*get_tie)(void* data),
    struct kedr_coi_creation_interceptor* interceptor,
    size_t operation_offset)
{
    void (*op_chained)(void* object, void* data);
    
    kedr_coi_bind_object(interceptor,
        object,
        get_tie(data),
        operation_offset,
        (void**)&op_chained); 

    //pr_info("In creation intermediate operation 'op_chained' is %pF.", op_chained);
    if(op_chained != NULL) op_chained(object, data);
}

#define KEDR_COI_TEST_DEFINE_CREATION_INTERMEDIATE_FUNC(func_name, get_tie, operation_offset, interceptor)  \
void func_name(void* object, void* data) \
{ \
    creation_intermediate_func_common(object, data, get_tie, interceptor, operation_offset); \
}


#endif /* KEDR_COI_TEST_HARNESS_CREATION */
