/*
 * Test different indirect interceptors for watch for the same object.
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


int op_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op_orig, op_call_counter);

struct test_operations test_operations_orig =
{
    .op = op_orig,
};

struct kedr_coi_interceptor* interceptor;
struct kedr_coi_interceptor* interceptor_another;

KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op_repl, OPERATION_OFFSET(op), interceptor);
KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op_repl_another, OPERATION_OFFSET(op), interceptor_another);

static struct kedr_coi_intermediate intermediate_operations[] =
{
    INTERMEDIATE(op, op_repl),
    INTERMEDIATE_FINAL
};

static struct kedr_coi_intermediate intermediate_operations_another[] =
{
    INTERMEDIATE(op, op_repl_another),
    INTERMEDIATE_FINAL
};


int op_pre1_call_counter;
KEDR_COI_TEST_DEFINE_HANDLER_FUNC(op_pre1, op_pre1_call_counter)

static struct kedr_coi_handler pre_handlers[] =
{
    HANDLER(op, op_pre1),
    kedr_coi_handler_end
};

static struct kedr_coi_payload payload =
{
    .pre_handlers = pre_handlers,
};

static struct kedr_coi_payload payload_another =
{
    .pre_handlers = pre_handlers,
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
    
    interceptor_another = INDIRECT_CONSTRUCTOR("Another indirect interceptor",
        offsetof(struct test_object, ops),
        sizeof(struct test_operations),
        intermediate_operations_another);
    
    if(interceptor_another == NULL)
    {
        pr_err("Failed to create another interceptor for test.");
        kedr_coi_interceptor_destroy(interceptor);
        return -EINVAL;
    }

    return 0;
}
void test_cleanup(void)
{
    kedr_coi_interceptor_destroy(interceptor_another);
    kedr_coi_interceptor_destroy(interceptor);
}

// Test itself
int test_run(void)
{
    int result;
    struct test_object object = {.ops = &test_operations_orig};
    
    result = kedr_coi_payload_register(interceptor, &payload);
    
    if(result)
    {
        pr_err("Failed to register payload.");
        goto err_payload;
    }
    
    result = kedr_coi_payload_register(interceptor_another, &payload_another);
    
    if(result)
    {
        pr_err("Failed to register payload for another interceptor.");
        goto err_payload_another;
    }


    result = kedr_coi_interceptor_start(interceptor);
    if(result)
    {
        pr_err("Interceptor failed to start.");
        goto err_start;
    }
    
    result = kedr_coi_interceptor_start(interceptor_another);
    if(result)
    {
        pr_err("Another interceptor failed to start.");
        goto err_start_another;
    }


    result = kedr_coi_interceptor_watch(interceptor, &object);
    if(result < 0)
    {
        pr_err("Interceptor failed to watch for an object.");
        goto err_watch;
    }

    result = kedr_coi_interceptor_watch(interceptor_another, &object);
    if(result >= 0)
    {
        pr_err("Another interceptor can watch for the same object. This is an error.");
        kedr_coi_interceptor_forget(interceptor_another, &object);
        result = -EINVAL;
        goto err_test;
    }

    kedr_coi_interceptor_forget(interceptor, &object);
    kedr_coi_interceptor_stop(interceptor_another);
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor_another, &payload_another);
    kedr_coi_payload_unregister(interceptor, &payload);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object);
err_watch:
    kedr_coi_interceptor_stop(interceptor_another);
err_start_another:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    kedr_coi_payload_unregister(interceptor_another, &payload_another);
err_payload_another:
    kedr_coi_payload_unregister(interceptor, &payload);
err_payload:
    return result;
}
