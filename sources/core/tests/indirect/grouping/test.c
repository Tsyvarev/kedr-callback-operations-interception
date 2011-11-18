/*
 * Test functuonality of indirect interceptor concerned with
 * grouping of intermediate operations.
 */

#include <kedr-coi/operations_interception.h>

#define OPERATION_OFFSET(op_name) offsetof(struct test_operations, op_name)
#include "test_harness.h"

#define INTERMEDIATE_GROUPED(op_name, op_repl, _group_id) \
{.operation_offset = OPERATION_OFFSET(op_name), .repl = op_repl, .group_id = _group_id}

/* Operations for test */
struct test_operations
{
    void* some_field;
    kedr_coi_test_op_t op1;
    void* other_fields[5];
    kedr_coi_test_op_t op2;
    void* another_fields[7];
    kedr_coi_test_op_t op3;
    kedr_coi_test_op_t op4;
    kedr_coi_test_op_t op5;
    kedr_coi_test_op_t op6;
};


struct test_object
{
    int some_field;
    const struct test_operations* ops;
};


struct test_operations test_operations_orig =
{
};

struct kedr_coi_interceptor* interceptor;

KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op1_repl, OPERATION_OFFSET(op1), interceptor);
KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op2_repl, OPERATION_OFFSET(op2), interceptor);
KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op3_repl, OPERATION_OFFSET(op3), interceptor);
KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op4_repl, OPERATION_OFFSET(op4), interceptor);
KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op5_repl, OPERATION_OFFSET(op5), interceptor);
KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op6_repl, OPERATION_OFFSET(op6), interceptor);

static struct kedr_coi_intermediate intermediate_operations[] =
{
    INTERMEDIATE_GROUPED(op1, op1_repl, 1),
    INTERMEDIATE_GROUPED(op2, op2_repl, 1),
    INTERMEDIATE_GROUPED(op3, op3_repl, 2),
    INTERMEDIATE_GROUPED(op4, op4_repl, 2),
    INTERMEDIATE_GROUPED(op5, op5_repl, 3),
    INTERMEDIATE_GROUPED(op6, op6_repl, 3),
    INTERMEDIATE_FINAL
};


// First payload with 2 handlers
int op1_pre1_call_counter;
KEDR_COI_TEST_DEFINE_PRE_HANDLER_FUNC(op1_pre1, op1_pre1_call_counter)

int op4_post1_call_counter;
KEDR_COI_TEST_DEFINE_POST_HANDLER_FUNC(op4_post1, op4_post1_call_counter)


static struct kedr_coi_pre_handler pre_handlers1[] =
{
    PRE_HANDLER_EXTERNAL(op1, op1_pre1),
    kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler post_handlers1[] =
{
    POST_HANDLER_EXTERNAL(op4, op4_post1),
    kedr_coi_post_handler_end
};


static struct kedr_coi_payload payload1 =
{
    .pre_handlers = pre_handlers1,
    .post_handlers = post_handlers1
};

// Second payload with 4 handlers
int op1_pre2_call_counter;
KEDR_COI_TEST_DEFINE_PRE_HANDLER_FUNC(op1_pre2, op1_pre2_call_counter)

int op3_pre2_call_counter;
KEDR_COI_TEST_DEFINE_PRE_HANDLER_FUNC(op3_pre2, op3_pre2_call_counter)

int op3_post2_call_counter;
KEDR_COI_TEST_DEFINE_POST_HANDLER_FUNC(op3_post2, op3_post2_call_counter)

int op3_post2_1_call_counter;
KEDR_COI_TEST_DEFINE_POST_HANDLER_FUNC(op3_post2_1, op3_post2_1_call_counter)

static struct kedr_coi_pre_handler pre_handlers2[] =
{
    PRE_HANDLER_EXTERNAL(op1, op1_pre2),
    PRE_HANDLER_EXTERNAL(op3, op3_pre2),
    kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler post_handlers2[] =
{
    POST_HANDLER_EXTERNAL(op3, op3_post2),
    POST_HANDLER_EXTERNAL(op3, op3_post2_1),
    kedr_coi_post_handler_end
};

static struct kedr_coi_payload payload2 =
{
    .pre_handlers = pre_handlers2,
    .post_handlers = post_handlers2
};


//******************Test infrastructure**********************************//
int test_init(void)
{
    interceptor = INDIRECT_CONSTRUCTOR("Indirect interceptor with several payloads",
        offsetof(struct test_object, ops),
        sizeof(struct test_operations),
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
    struct test_object object = {.ops = &test_operations_orig};
    
    result = kedr_coi_payload_register(interceptor, &payload1);
    
    if(result)
    {
        pr_err("Failed to register payload 1.");
        goto err_payload1;
    }
    
    result = kedr_coi_payload_register(interceptor, &payload2);
    
    if(result)
    {
        pr_err("Failed to register payload 2.");
        goto err_payload2;
    }

    result = kedr_coi_interceptor_start(interceptor);
    if(result)
    {
        pr_err("Interceptor failed to start.");
        goto err_start;
    }
    
    result = kedr_coi_interceptor_watch(interceptor, &object);
    if(result < 0)
    {
        pr_err("Interceptor failed to watch for an object.");
        goto err_watch;
    }

    // There are no handlers for operation 2, but intermediate operation
    // for it is grouped with 1, for which there is 1 handler
  
    if(object.ops->op2 == NULL)
    {
        pr_err("Intermediate wasn't used for operation2(NULL), while it "
            "is grouped with operation1(NULL), for which external "
            "interception handler exist.");
        result = -EINVAL;
        goto err_test;
    }
    
    //Simply call, for check that nothing will be crashed
    object.ops->op2(&object, NULL);
    
    // Operations 5 and 6 are grouped, but neither of them has handlers
    if(object.ops->op5 != NULL)
    {
        pr_err("Operation 5 was changed even without handlers.");
        result = -EINVAL;
        goto err_test;
    }

    if(object.ops->op6 != NULL)
    {
        pr_err("Operation 6 was changed even without handlers.");
        result = -EINVAL;
        goto err_test;
    }


    kedr_coi_interceptor_forget(interceptor, &object);
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor, &payload2);
    kedr_coi_payload_unregister(interceptor, &payload1);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object);
err_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    kedr_coi_payload_unregister(interceptor, &payload2);
err_payload2:
    kedr_coi_payload_unregister(interceptor, &payload1);
err_payload1:
    return result;
}