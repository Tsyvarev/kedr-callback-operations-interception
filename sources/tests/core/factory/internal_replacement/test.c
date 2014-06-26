/*
 * Test functionality of factory interceptor in case of NULL operation
 * pointer and internal normal replacement.
 * (Factory replacements are always external.)
 */

#include <kedr-coi/operations_interception.h>

#define OPERATION_OFFSET(op_name) offsetof(struct test_operations, op_name)
#include "test_harness.h"
#include "test_harness_factory.h"

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

int op2_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op2_orig, op2_call_counter);

struct test_operations test_operations_orig =
{
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

// Prototype object and factory interceptor
struct test_factory
{
    int some_another_fields[7];
    const struct test_operations* factory_ops;
};

static void* get_factory(void* data)
{
    return data;
}

struct kedr_coi_factory_interceptor* factory_interceptor;

KEDR_COI_TEST_DEFINE_FACTORY_INTERMEDIATE_FUNC(op1_factory_repl,
    get_factory, OPERATION_OFFSET(op1), factory_interceptor);

static struct kedr_coi_intermediate factory_intermediate_operations[] =
{
    INTERMEDIATE(op1, op1_factory_repl),
    INTERMEDIATE_FINAL
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
    
    factory_interceptor = kedr_coi_factory_interceptor_create(
        interceptor,
        "Simple factory interceptor",
        offsetof(struct test_factory, factory_ops),
        factory_intermediate_operations);
    
    if(factory_interceptor == NULL)
    {
        pr_err("Failed to create factory interceptor for test.");
        kedr_coi_interceptor_destroy(interceptor);
        return -EINVAL;
    }


    return 0;
}
void test_cleanup(void)
{
    kedr_coi_factory_interceptor_destroy(factory_interceptor);
    kedr_coi_interceptor_destroy(interceptor);
}

// Test itself
int test_run(void)
{
    int result;
    struct test_factory factory = {.factory_ops = &test_operations_orig};
    struct test_object object;
    
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
    
    result = kedr_coi_factory_interceptor_watch(factory_interceptor,
        &factory);
    if(result < 0)
    {
        pr_err("Factory interceptor failed to watch for an object.");
        goto err_factory_watch;
    }

    // As if normal object was created from prototype and its 
    // operation 1 was called.
    object.ops = factory.factory_ops;

    op1_pre1_call_counter = 0;
    
    object.ops->op1(&object, &factory);
    
    if(op1_pre1_call_counter != 0)
    {
        pr_err("Internal pre handler for operation 1(NULL) was called.");
        result = -EINVAL;
        goto err_test;
    }

    // Check another operation
    op2_call_counter = 0;
    op2_post1_call_counter = 0;
    
    object.ops->op2(&object, NULL);
    
    if(op2_post1_call_counter == 0)
    {
        pr_err("Post handler for operation 2 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op2_call_counter == 0)
    {
        pr_err("Original operation 2 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }

    result = kedr_coi_interceptor_forget(interceptor, &object);
    if(result < 0)
    {
        pr_err("Error occured when forget normal object.");
        goto err_forget;
    }
    if(result == 1)
    {
        pr_err("Normal object should be automatically watched, but 'forget' return 1.");
        result = -EINVAL;
        goto err_forget;
    }

    kedr_coi_factory_interceptor_forget(factory_interceptor, &factory);
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor, &payload);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object);
err_forget:
    kedr_coi_factory_interceptor_forget(factory_interceptor, &factory);
err_factory_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    kedr_coi_payload_unregister(interceptor, &payload);
err_payload:
    return result;
}
