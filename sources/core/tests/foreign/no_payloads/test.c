/*
 * Test functionality of foreign interceptor with no payloads.
 */

#include <kedr-coi/operations_interception.h>

#define OPERATION_OFFSET(op_name) offsetof(struct test_operations, op_name)
#include "test_harness.h"
#include "test_harness_foreign.h"

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


// Prototype object and foreign interceptor
struct test_prototype_object
{
    int some_another_fields[7];
    const struct test_operations* foreign_ops;
};

static void* get_prototype_object(void* data)
{
    return data;
}

struct kedr_coi_foreign_interceptor* foreign_interceptor;

KEDR_COI_TEST_DEFINE_FOREIGN_INTERMEDIATE_FUNC(op1_foreign_repl,
    get_prototype_object, OPERATION_OFFSET(op1), foreign_interceptor);

static struct kedr_coi_foreign_intermediate foreign_intermediate_operations[] =
{
    INTERMEDIATE(op1, op1_foreign_repl),
    INTERMEDIATE_FINAL
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
    
    foreign_interceptor = kedr_coi_foreign_interceptor_create(
        interceptor,
        "Simple foreign interceptor",
        offsetof(struct test_prototype_object, foreign_ops),
        foreign_intermediate_operations,
        NULL);
    
    if(foreign_interceptor == NULL)
    {
        pr_err("Failed to create foreign interceptor for test.");
        kedr_coi_interceptor_destroy(interceptor);
        return -EINVAL;
    }


    return 0;
}
void test_cleanup(void)
{
    kedr_coi_foreign_interceptor_destroy(foreign_interceptor);
    kedr_coi_interceptor_destroy(interceptor);
}

// Test itself
int test_run(void)
{
    int result;
    struct test_prototype_object prototype_object = {.foreign_ops = &test_operations_orig};
    struct test_object object;
    
    result = kedr_coi_interceptor_start(interceptor);
    if(result)
    {
        pr_err("Interceptor failed to start.");
        goto err_start;
    }
    
    result = kedr_coi_foreign_interceptor_watch(foreign_interceptor,
        &prototype_object);
    if(result < 0)
    {
        pr_err("Foreign interceptor failed to watch for an object.");
        goto err_foreign_watch;
    }

    // As if normal object was created from prototype and its 
    // operation 1 was called.
    object.ops = prototype_object.foreign_ops;

    op1_call_counter = 0;
    
    object.ops->op1(&object, &prototype_object);
    
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
    // It is allowable for foreign interceptors do not automatically watch
    // for an object, if normal interceptor binded with it has no handlers
    //if(result == 1)
    //{
        //pr_err("Normal object should be automatically watched, but 'forget' return 1.");
        //result = -EINVAL;
        //goto err_forget;
    //}

    kedr_coi_foreign_interceptor_forget(foreign_interceptor, &prototype_object);
    kedr_coi_interceptor_stop(interceptor);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object);
err_forget:
    kedr_coi_foreign_interceptor_forget(foreign_interceptor, &prototype_object);
err_foreign_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    return result;
}