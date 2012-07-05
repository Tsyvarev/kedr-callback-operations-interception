/*
 *  Test that external interception handler cannot be set for callback,
 * which allow only internal interception ('internal_only' flag is set).
 */

#include <kedr-coi/operations_interception.h>

#define OPERATION_OFFSET(op_name) offsetof(struct test_object, op_name)
#include "test_harness.h"

/* Object with operations for test*/
struct test_object
{
    void* some_field;
    kedr_coi_test_op_t op1;
    void* other_fields[5];
    kedr_coi_test_op_t op2;
};


int op2_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op2_orig, op2_call_counter);

struct kedr_coi_interceptor* interceptor;

KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op1_repl, OPERATION_OFFSET(op1), interceptor);
KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op2_repl, OPERATION_OFFSET(op2), interceptor);

static struct kedr_coi_intermediate intermediate_operations[] =
{
    INTERMEDIATE_INTERNAL_ONLY(op1, op1_repl),
    INTERMEDIATE(op2, op2_repl),
    INTERMEDIATE_FINAL
};


int op1_pre1_call_counter;
KEDR_COI_TEST_DEFINE_PRE_HANDLER_FUNC(op1_pre1, op1_pre1_call_counter)

int op1_pre2_call_counter;
KEDR_COI_TEST_DEFINE_PRE_HANDLER_FUNC(op1_pre2, op1_pre2_call_counter)


int op2_post1_call_counter;
KEDR_COI_TEST_DEFINE_POST_HANDLER_FUNC(op2_post1, op2_post1_call_counter)

static struct kedr_coi_pre_handler pre_handlers[] =
{
    PRE_HANDLER(op1, op1_pre1),
    PRE_HANDLER_EXTERNAL(op1, op1_pre2),
    kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler post_handlers[] =
{
    POST_HANDLER(op2, op2_post1),
    kedr_coi_post_handler_end
};

static struct kedr_coi_payload payload =
{
    .pre_handlers = pre_handlers,
    .post_handlers = post_handlers
};


//******************Test infrastructure**********************************//
int test_init(void)
{
    interceptor = kedr_coi_interceptor_create_direct(
        "Simple direct interceptor",
        sizeof(struct test_object),
        intermediate_operations,
        NULL);
    
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
        pr_err("External handler has been set for callback, "
            "which allows only internal interception.");
        kedr_coi_payload_unregister(interceptor, &payload);
        return -EINVAL;
    }

    return 0;
}