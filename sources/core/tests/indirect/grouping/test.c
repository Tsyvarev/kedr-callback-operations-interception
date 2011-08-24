/*
 * Test functuonality of indirect interceptor concerned
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


int op1_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op1_orig, op1_call_counter);

int op2_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op2_orig, op2_call_counter);

int op3_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op3_orig, op3_call_counter);

int op4_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op4_orig, op4_call_counter);

int op5_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op5_orig, op5_call_counter);

int op6_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op6_orig, op6_call_counter);



struct test_operations test_operations_orig =
{
    .op1 = op1_orig,
    .op2 = op2_orig,
    .op3 = op3_orig,
    .op4 = op4_orig,
    .op5 = op5_orig,
    .op6 = op6_orig
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
    PRE_HANDLER(op1, op1_pre1),
    kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler post_handlers1[] =
{
    POST_HANDLER(op4, op4_post1),
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
    PRE_HANDLER(op1, op1_pre2),
    PRE_HANDLER(op3, op3_pre2),
    kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler post_handlers2[] =
{
    POST_HANDLER(op3, op3_post2),
    POST_HANDLER(op3, op3_post2_1),
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
    interceptor = kedr_coi_interceptor_create("Indirect interceptor with several payloads",
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

    op1_call_counter = 0;
    op1_pre1_call_counter = 0;
    op1_pre2_call_counter = 0;
    
    object.ops->op1(&object, NULL);
    
    if(op1_pre1_call_counter == 0)
    {
        pr_err("Pre handler 1 for operation 1 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op1_pre2_call_counter == 0)
    {
        pr_err("Pre handler 2 for operation 1 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op1_call_counter == 0)
    {
        pr_err("Original operation 1 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }


    op3_call_counter = 0;
    op3_pre2_call_counter = 0;
    op3_post2_call_counter = 0;
    op3_post2_1_call_counter = 0;
    
    object.ops->op3(&object, NULL);
    
    if(op3_pre2_call_counter == 0)
    {
        pr_err("Pre handler 2 for operation 3 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op3_post2_call_counter == 0)
    {
        pr_err("Post handler 2 for operation 3 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op3_post2_1_call_counter == 0)
    {
        pr_err("Post handler 2_1 for operation 3 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op3_call_counter == 0)
    {
        pr_err("Original operation 3 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }

    
    op4_call_counter = 0;
    op4_post1_call_counter = 0;
    
    object.ops->op4(&object, NULL);
    
    if(op4_post1_call_counter == 0)
    {
        pr_err("Post handler 1 for operation 4 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op4_call_counter == 0)
    {
        pr_err("Original operation 4 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }


    // There are no handlers for operation 2, but intermediate operation
    // for it is grouped with 1, for which there is 1 handler
    op2_call_counter = 0;
    
    object.ops->op2(&object, NULL);
    
    if(op2_call_counter == 0)
    {
        pr_err("Original operation 2 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }

    if(object.ops->op2 != op2_repl)
    {
        pr_err("Operation 2 wasn't replaced while it grouped with operation 1, which has handlers.");
        result = -EINVAL;
        goto err_test;
    }

    // Operations 5 and 6 are grouped, but neither of them has handlers
    if(object.ops->op5 != op5_orig)
    {
        pr_err("Operation 5 was changed even without handlers.");
        result = -EINVAL;
        goto err_test;
    }

    if(object.ops->op6 != op6_orig)
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