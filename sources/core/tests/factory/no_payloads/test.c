/*
 * Test functionality of factory interceptor with no payloads.
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

static struct kedr_coi_factory_intermediate factory_intermediate_operations[] =
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
        intermediate_operations,
        NULL);
    
    if(interceptor == NULL)
    {
        pr_err("Failed to create interceptor for test.");
        return -EINVAL;
    }
    
    factory_interceptor = kedr_coi_factory_interceptor_create(
        interceptor,
        "Simple factory interceptor",
        offsetof(struct test_factory, factory_ops),
        factory_intermediate_operations,
        NULL);
    
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

    op1_call_counter = 0;
    
    object.ops->op1(&object, &factory);
    
    if(op1_call_counter == 0)
    {
        pr_err("Original operation 1 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }

    // Check another operation
    op2_call_counter = 0;
    
    object.ops->op2(&object, NULL);
    
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
    // It is allowable for factory interceptors do not automatically watch
    // for an object, if normal interceptor binded with it has no handlers
    //if(result == 1)
    //{
        //pr_err("Normal object should be automatically watched, but 'forget' return 1.");
        //result = -EINVAL;
        //goto err_forget;
    //}

    kedr_coi_factory_interceptor_forget(factory_interceptor, &factory);
    kedr_coi_interceptor_stop(interceptor);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object);
err_forget:
    kedr_coi_factory_interceptor_forget(factory_interceptor, &factory);
err_factory_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    return result;
}