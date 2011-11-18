/*
 * Test whether indirect interceptor correctly update watching for the object.
 */

#include <kedr-coi/operations_interception.h>

#define OPERATION_OFFSET(op_name) offsetof(struct test_operations, op_name)
#include "test_harness.h"

/* Operations for test */
struct test_operations
{
    void* some_field;
    kedr_coi_test_op_t op;
    void* other_fields[5];
};


struct test_object
{
    int some_field;
    const struct test_operations* ops;
};


int op_call_counter1 = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op_orig1, op_call_counter1);

int op_call_counter2 = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op_orig2, op_call_counter2);

struct test_operations test_operations_orig1 =
{
    .op = op_orig1,
};

struct test_operations test_operations_orig2 =
{
    .op = op_orig2,
};


struct kedr_coi_interceptor* interceptor;

KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op_repl, OPERATION_OFFSET(op), interceptor);

static struct kedr_coi_intermediate intermediate_operations[] =
{
    INTERMEDIATE(op, op_repl),
    INTERMEDIATE_FINAL
};


int op_pre_call_counter;
KEDR_COI_TEST_DEFINE_PRE_HANDLER_FUNC(op_pre, op_pre_call_counter)

static struct kedr_coi_pre_handler pre_handlers[] =
{
    PRE_HANDLER(op, op_pre),
    kedr_coi_pre_handler_end
};

static struct kedr_coi_payload payload =
{
    .pre_handlers = pre_handlers
};


//******************Test infrastructure**********************************//
int test_init(void)
{
    interceptor = INDIRECT_CONSTRUCTOR("Simple indirect interceptor",
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
    struct test_object object = {.ops = &test_operations_orig1};
    
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
    // Simple another call of 'watch'
    result = kedr_coi_interceptor_watch(interceptor, &object);
    if(result < 0)
    {
        pr_err("Interceptor failed to update watching for an object.");
        goto err_test;
    }

    if(result != 1)
    {
        pr_err("kedr_coi_interceptor_watch() should return 1 "
                "if watching was updated, but it returns %d.",
                result);
        result = -EINVAL;
        goto err_test;
    }

    op_call_counter1 = 0;
    op_pre_call_counter = 0;
    
    object.ops->op(&object, NULL);
    
    if(op_pre_call_counter == 0)
    {
        pr_err("Pre handler for operation wasn't called after updating watching.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op_call_counter1 == 0)
    {
        pr_err("Original operation wasn't called after updating watching.");
        result = -EINVAL;
        goto err_test;
    }
    
    // As if operations was reset outside of the interceptor
    object.ops = &test_operations_orig1;

    result = kedr_coi_interceptor_watch(interceptor, &object);
    if(result < 0)
    {
        pr_err("Interceptor failed to update watching for an object when operations was reset.");
        goto err_test;
    }

    if(result != 1)
    {
        pr_err("kedr_coi_interceptor_watch() should return 1 "
                "if watching was updated(reseted), but it returns %d.",
                result);
        result = -EINVAL;
        goto err_test;
    }

    op_call_counter1 = 0;
    op_pre_call_counter = 0;
    
    object.ops->op(&object, NULL);
    
    if(op_pre_call_counter == 0)
    {
        pr_err("Pre handler for operation wasn't called after updating watching(reset).");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op_call_counter1 == 0)
    {
        pr_err("Original operation wasn't called after updating watching(reset).");
        result = -EINVAL;
        goto err_test;
    }

    // Operations in the object was set to another ones
    // (Possible in the open() operation of 'struct file').
    object.ops = &test_operations_orig2;

    result = kedr_coi_interceptor_watch(interceptor, &object);
    if(result < 0)
    {
        pr_err("Interceptor failed to update watching for an object "
            "when operations was set to new ones.");
        goto err_test;
    }

    if(result != 1)
    {
        pr_err("kedr_coi_interceptor_watch() should return 1 "
                "if watching was updated(changed), but it returns %d.",
                result);
        result = -EINVAL;
        goto err_test;
    }

    op_call_counter2 = 0;
    op_pre_call_counter = 0;
    
    object.ops->op(&object, NULL);
    
    if(op_pre_call_counter == 0)
    {
        pr_err("Pre handler for operation wasn't called after updating watching(changed).");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op_call_counter2 == 0)
    {
        pr_err("Original operation wasn't called after updating watching(changed).");
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