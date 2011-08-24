/*
 * Infrastructure for test callback operation interception(foreign functionality).
 */

#ifndef KEDR_COI_TEST_HARNESS_FOREIGN
#define KEDR_COI_TEST_HARNESS_FOREIGN

#include "test_harness.h"

static inline void foreign_intermediate_func_common(void* object,
    void* data,
    void* (*get_prototype_object)(void* data),
    struct kedr_coi_foreign_interceptor* interceptor,
    size_t operation_offset)
{
    void (*op_chained)(void* object, void* data);
    
    kedr_coi_bind_prototype_with_object(interceptor,
        get_prototype_object(data),
        object,
        operation_offset,
        (void**)&op_chained); 

    //pr_info("In foreign intermediate operation 'op_chained' is %pF.", op_chained);
    if(op_chained != NULL) op_chained(object, data);
}

#define KEDR_COI_TEST_DEFINE_FOREIGN_INTERMEDIATE_FUNC(func_name, get_prototype_object, operation_offset, interceptor)  \
void func_name(void* object, void* data) \
{ \
    foreign_intermediate_func_common(object, data, get_prototype_object, interceptor, operation_offset); \
}


#endif /* KEDR_COI_TEST_HARNESS_FOREIGN */
