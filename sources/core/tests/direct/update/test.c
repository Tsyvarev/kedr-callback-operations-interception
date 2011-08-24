/*
 * Test whether direct interceptor correctly update watching for the object.
 */

#include <kedr-coi/operations_interception.h>

#define OPERATION_OFFSET(op_name) offsetof(struct test_object, op_name)
#include "test_harness.h"

/* Object with operations for test */
struct test_object
{
    void* some_field;
    kedr_coi_test_op_t op;
    void* other_fields[5];
};


int op_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op_orig, op_call_counter);

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
    struct test_object object = {.op = op_orig};
    
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

    // Another call to 'watch'
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

    op_call_counter = 0;
    op_pre_call_counter = 0;
    
    object.op(&object, NULL);
    
    if(op_pre_call_counter == 0)
    {
        pr_err("Pre handler for operation wasn't called after updating watching.");
        result = EINVAL;
        goto err_test;
    }
    
    if(op_call_counter == 0)
    {
        pr_err("Original operation wasn't called after updating watching.");
        result = EINVAL;
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