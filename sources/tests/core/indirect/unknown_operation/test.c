/*
 * Test that payload cannot be registered if it requires to intercept
 * operation which is not known by interceptor.
 */

#include <kedr-coi/operations_interception.h>

#define OPERATION_OFFSET(op_name) offsetof(struct test_operations, op_name)
#include "test_harness.h"

/* Operations for test */
struct test_operations
{
    void* some_field;
    kedr_coi_test_op_t op1;
    void* other_fields[5];
    kedr_coi_test_op_t op2;
};


struct test_object
{
    int some_field;
    const struct test_operations* ops;
};


struct kedr_coi_interceptor* interceptor;

KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op1_repl, OPERATION_OFFSET(op1), interceptor)

static struct kedr_coi_intermediate intermediate_operations[] =
{
    INTERMEDIATE(op1, op1_repl),
    INTERMEDIATE_FINAL
};


int op2_post_call_counter;
KEDR_COI_TEST_DEFINE_HANDLER_FUNC(op2_post, op2_post_call_counter)

static struct kedr_coi_handler post_handlers[] =
{
    HANDLER(op2, op2_post),
    kedr_coi_handler_end
};

static struct kedr_coi_payload payload =
{
    .post_handlers = post_handlers
};


//******************Test infrastructure**********************************//
int test_init(void)
{
    interceptor = INDIRECT_CONSTRUCTOR("Simple indirect interceptor",
        offsetof(struct test_object, ops),
        sizeof(struct test_operations),
        intermediate_operations);
    
    if(interceptor == NULL)
    {
        pr_err("Failed to create interceptor for test.");
        return -EINVAL;
    }
    
    return 0;
}
void test_cleanup(void)
{
    kedr_coi_interceptor_destroy(interceptor);
}

// Test itself
int test_run(void)
{
    int result;
    
    result = kedr_coi_payload_register(interceptor, &payload);
    
    if(result == 0)
    {
        pr_err("Registration of payload with unknown operation succeed.");
        kedr_coi_payload_unregister(interceptor, &payload);
        return -EINVAL;
    }

	return 0;
}
