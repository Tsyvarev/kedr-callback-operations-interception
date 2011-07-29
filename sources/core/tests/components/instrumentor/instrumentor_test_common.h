#ifndef INSTRUMENTOR_TEST_COMMON_H
#define INSTRUMENTOR_TEST_COMMON_H
/* Functions for verify instrumentation */

#include "kedr_coi_instrumentor_internal.h"

struct kedr_coi_test_replaced_operation
{
    size_t offset;
    void* orig;
    void* repl;
};

struct kedr_coi_test_unaffected_operation
{
    size_t offset;
    void* orig;
};

/*
 * Accept instrumented object and instrumentor itself.
 * 
 * Verify that given operations are correctly replaced,
 * other are unchanged.
 * 
 * 'operations_struct' should be a pointer to operations
 * of object(for indirect instrumentation) or pointer to object
 * itself(for direct instrumentation).
 * 
 * 'replaced_operations' is an array of operations which should be
 * replaced in the object. Last element in the array should have offset = -1.
 * 
 * 'unaffected_operations' is an array of operations which should remain
 * unchanged in the object. Last element in the array should have offset = -1.
 * 
 * Other content of the object will not be verified.
 */
int check_object_instrumented(const void* object,
    struct kedr_coi_instrumentor* instrumentor,
    const void* operations_struct,
    const struct kedr_coi_test_replaced_operation* replaced_operations,
    const struct kedr_coi_test_unaffected_operation* unaffected_operations);


/*
 * Accept not instrumented object.
 * 
 * Verify that given operations are all unchanged.
 * 
 * 'operations_struct' should be a pointer to operations
 * of object(for indirect instrumentation) or pointer to object
 * itself(for direct instrumentation).
 * 
 * 'replaced_operations' is an array of operations which may be
 * replaced in the object. Last element in the array should have offset = -1.
 * 
 * 'unaffected_operations' is an array of operations which should remain
 * unchanged in the object. Last element in the array should have offset = -1.
 * 
 * Other content of the object will not be verified.
 */

int check_object_uninstrumented(const void* object,
    const void* operations_struct,
    const struct kedr_coi_test_replaced_operation* replaced_operations,
    const struct kedr_coi_test_unaffected_operation* unaffected_operations);


#endif /* INSTRUMENTOR_TEST_COMMON_H */