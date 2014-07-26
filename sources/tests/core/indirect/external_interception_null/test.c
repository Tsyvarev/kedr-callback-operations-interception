/*
 *  Test that external interception handler is correctly set and 
 * called in case of NULL operations pointer.
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

KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op1_repl, OPERATION_OFFSET(op1), interceptor);
KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op2_repl, OPERATION_OFFSET(op2), interceptor);

static struct kedr_coi_intermediate intermediate_operations[] =
{
    INTERMEDIATE(op1, op1_repl),
    INTERMEDIATE(op2, op2_repl),
    INTERMEDIATE_FINAL
};


int op1_pre1_call_counter;
KEDR_COI_TEST_DEFINE_HANDLER_FUNC(op1_pre1, op1_pre1_call_counter)

int op2_post1_call_counter;
KEDR_COI_TEST_DEFINE_HANDLER_FUNC(op2_post1, op2_post1_call_counter)

int op2_post2_call_counter;
KEDR_COI_TEST_DEFINE_HANDLER_FUNC(op2_post2, op2_post2_call_counter)


static struct kedr_coi_handler pre_handlers[] =
{
    HANDLER(op1, op1_pre1),
    kedr_coi_handler_end
};

static struct kedr_coi_handler post_handlers[] =
{
    HANDLER(op2, op2_post1),
    HANDLER_EXTERNAL(op2, op2_post2),
    kedr_coi_handler_end
};

static struct kedr_coi_payload payload =
{
    .pre_handlers = pre_handlers,
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
    struct test_object object = {.ops = NULL};
    
    result = kedr_coi_payload_register(interceptor, &payload);
    
    if(result)
    {
        pr_err("Failed to register payload.");
        goto err_payload;
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

    if(object.ops->op1 != NULL)
    {
        pr_err("Operation is intercepted while it shouldn't(no original one).");
        result = -EINVAL;
        goto err_test;
    }
    
    op2_post1_call_counter = 0;
    op2_post2_call_counter = 0;
    
    object.ops->op2(&object, NULL);
    
    if(op2_post1_call_counter != 0)
    {
        pr_err("Internal post handler for operation 2(NULL) was called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op2_post2_call_counter == 0)
    {
        pr_err("External post handler for operation 2(NULL) wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    kedr_coi_interceptor_forget(interceptor, &object);
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor, &payload);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object);
err_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    kedr_coi_payload_unregister(interceptor, &payload);
err_payload:
    return result;
}
