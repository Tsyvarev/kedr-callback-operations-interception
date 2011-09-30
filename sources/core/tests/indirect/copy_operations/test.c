/*
 * Test that indirect interceptor is stable to copiing replaced operations
 * into another object.
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


int op1_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op1_orig, op1_call_counter);

int op2_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op2_orig, op2_call_counter);

struct test_operations test_operations_orig =
{
    .op1 = op1_orig,
    .op2 = op2_orig
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
KEDR_COI_TEST_DEFINE_PRE_HANDLER_FUNC(op1_pre1, op1_pre1_call_counter)

int op2_post1_call_counter;
KEDR_COI_TEST_DEFINE_POST_HANDLER_FUNC(op2_post1, op2_post1_call_counter)

static struct kedr_coi_pre_handler pre_handlers[] =
{
    PRE_HANDLER(op1, op1_pre1),
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
    interceptor = kedr_coi_interceptor_create("Simple indirect interceptor",
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
    struct test_object object_another = {.ops = &test_operations_orig};
    
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

    op1_call_counter = 0;
    op1_pre1_call_counter = 0;
    
    object.ops->op1(&object, NULL);
    
    if(op1_pre1_call_counter == 0)
    {
        pr_err("Pre handler for operation 1 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op1_call_counter == 0)
    {
        pr_err("Original operation 1 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }


    // As if another object was created at this moment and its operations
    // was copied from current object.
    object_another.ops = object.ops;

    // Another object is not watched, so handlers shouldn't be called
    // for its operations.

    op1_call_counter = 0;
    op1_pre1_call_counter = 0;
    
    object_another.ops->op1(&object_another, NULL);
    
    if(op1_pre1_call_counter != 0)
    {
        pr_err("Pre handler for operation 1 was called, but object is not watched.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op1_call_counter == 0)
    {
        pr_err("Original operation 1 wasn't called for object which is not watched.");
        result = -EINVAL;
        goto err_test;
    }
    // Now watch for another object
    result = kedr_coi_interceptor_watch(interceptor, &object_another);
    if(result < 0)
    {
        pr_err("Interceptor failed to watch for another object.");
        goto err_watch;
    }

    op1_call_counter = 0;
    op1_pre1_call_counter = 0;
    
    object_another.ops->op1(&object_another, NULL);
    
    if(op1_pre1_call_counter == 0)
    {
        pr_err("Pre handler for operation 1 wasn't called for another object.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op1_call_counter == 0)
    {
        pr_err("Original operation 1 wasn't called for another object.");
        result = -EINVAL;
        goto err_test;
    }

    kedr_coi_interceptor_forget(interceptor, &object_another);
    kedr_coi_interceptor_forget(interceptor, &object);
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor, &payload);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object_another);
    kedr_coi_interceptor_forget(interceptor, &object);
err_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    kedr_coi_payload_unregister(interceptor, &payload);
err_payload:
    return result;
}