/*
 * Test that callback for kedr_coi_interceptor_trace_unforgotten_object()
 * is called when needed with appropriate parameters.
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

struct test_operations test_operations_orig =
{
    .op1 = op1_orig,
};

struct kedr_coi_interceptor* interceptor;

KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op1_repl, OPERATION_OFFSET(op1), interceptor);

static struct kedr_coi_intermediate intermediate_operations[] =
{
    INTERMEDIATE(op1, op1_repl),
    INTERMEDIATE_FINAL
};


int op1_pre1_call_counter;
KEDR_COI_TEST_DEFINE_HANDLER_FUNC(op1_pre1, op1_pre1_call_counter)

static struct kedr_coi_handler pre_handlers[] =
{
    HANDLER(op1, op1_pre1),
    kedr_coi_handler_end
};

static struct kedr_coi_payload payload =
{
    .pre_handlers = pre_handlers,
};

static struct test_object object = {.ops = &test_operations_orig};

static int callback_counter = 0;
static bool cb_err = 0;

static void cb(const void* obj)
{
    if(!cb_err)
    {
        callback_counter++;
        
        if(obj != &object)
        {
            pr_info("Trace unforgotten object callback has been called "
                "with object %p, but should be called with object %p.",
                obj, &object);
            cb_err = 1;
        }
    }
}

//******************Test infrastructure**********************************//
int test_init(void)
{
    int result;
    
    interceptor = INDIRECT_CONSTRUCTOR(
        "Simple indirect interceptor",
        offsetof(struct test_object, ops),
        sizeof(struct test_operations),
        intermediate_operations);
    
    if(interceptor == NULL)
    {
        pr_err("Failed to create interceptor for test.");
        return -EINVAL;
    }
    
    kedr_coi_interceptor_trace_unforgotten_object(interceptor, cb);
    
    result = kedr_coi_payload_register(interceptor, &payload);
    
    if(result)
    {
        pr_err("Failed to register payload.");
        goto err_payload;
    }
    
    return 0;

err_payload:
    kedr_coi_interceptor_destroy(interceptor);
    
    return result;

}
void test_cleanup(void)
{
    kedr_coi_payload_unregister(interceptor, &payload);
    kedr_coi_interceptor_destroy(interceptor);
}

// Test itself
int test_run(void)
{
    int result;
    
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

    
    kedr_coi_interceptor_stop(interceptor);

    if(cb_err) return -EINVAL;
    if(callback_counter == 0)
    {
        pr_info("Trace unforgotten object callback has not been called.");
        return -EINVAL;
    }

    if(callback_counter > 1)
    {
        pr_info("Trace unforgotten object callback has been called %d times, "
            "but should be called only once.", callback_counter);
        return -EINVAL;
    }
    
    return 0;

err_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    return result;
}
