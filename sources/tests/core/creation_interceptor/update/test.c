/*
 * Test functionality of creation interceptor when update replacements.
 */

#include <kedr-coi/operations_interception.h>

#define OPERATION_OFFSET(op_name) offsetof(struct test_operations, op_name)
#include "test_harness.h"
#include "test_harness_creation.h"

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


int op1_call_counter_another = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op1_orig_another, op1_call_counter_another);

int op2_call_counter_another = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op2_orig_another, op2_call_counter_another);

struct test_operations test_operations_orig_another =
{
    .op1 = op1_orig_another,
    .op2 = op2_orig_another
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

static struct kedr_coi_handler pre_handlers[] =
{
    HANDLER(op1, op1_pre1),
    kedr_coi_handler_end
};

static struct kedr_coi_handler post_handlers[] =
{
    HANDLER(op2, op2_post1),
    kedr_coi_handler_end
};

static struct kedr_coi_payload payload =
{
    .pre_handlers = pre_handlers,
    .post_handlers = post_handlers
};

// Creation interceptor
static void* get_tie(void* data)
{
    return data;
}

struct kedr_coi_factory_interceptor* creation_interceptor;

KEDR_COI_TEST_DEFINE_CREATION_INTERMEDIATE_FUNC(op1_creation_repl,
    get_tie, OPERATION_OFFSET(op1), creation_interceptor);

static struct kedr_coi_intermediate creation_intermediate_operations[] =
{
    INTERMEDIATE(op1, op1_creation_repl),
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
    
    creation_interceptor = kedr_coi_factory_interceptor_create_generic(
        interceptor,
        "Simple creation interceptor",
        creation_intermediate_operations);
    
    if(creation_interceptor == NULL)
    {
        pr_err("Failed to create creation interceptor for test.");
        kedr_coi_interceptor_destroy(interceptor);
        return -EINVAL;
    }

    return 0;
}
void test_cleanup(void)
{
    kedr_coi_factory_interceptor_destroy(creation_interceptor);
    kedr_coi_interceptor_destroy(interceptor);
}

// Test itself
int test_run(void)
{
    int result;
    const struct test_operations* object_operations =
        &test_operations_orig;
    void* id = (void*)0x123;
    void* tie = (void*)0x654;

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
    
    result = kedr_coi_factory_interceptor_watch_generic(creation_interceptor,
        id, tie, (const void**)&object_operations);
    if(result < 0)
    {
        pr_err("Creation interceptor failed to watch operations.");
        goto err_creation_watch;
    }
    // Simply call 'watch' again
    result = kedr_coi_factory_interceptor_watch_generic(creation_interceptor,
        id, tie, (const void**)&object_operations);
    if(result < 0)
    {
        pr_err("Creation interceptor failed to update watching operations.");
        goto err_test;
    }

    // As if operations for the prototype object was reseted.
    object_operations = &test_operations_orig;

    result = kedr_coi_factory_interceptor_watch_generic(creation_interceptor,
        id, tie, (const void**)&object_operations);
    if(result < 0)
    {
        pr_err("Creation interceptor failed to update watching "
            "operations after them was reseted.");
        goto err_test;
    }

    // As if normal object was created from watched operations and its 
    // operation 1 was called.
    object.ops = object_operations;

    op1_call_counter = 0;
    op1_pre1_call_counter = 0;
    
    object.ops->op1(&object, tie);
    
    if(op1_call_counter == 0)
    {
        pr_err("Original operation 1 wasn't called.");
        result = -EINVAL;
        goto err_test;
    }

    if(op1_pre1_call_counter == 0)
    {
        pr_err("Pre handler for operation 1 wasn't called.");
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

    kedr_coi_factory_interceptor_forget_generic(creation_interceptor,
        id, (const void**)&object_operations);
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor, &payload);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object);
err_forget:
    kedr_coi_factory_interceptor_forget_generic(creation_interceptor,
        id, (const void**)&object_operations);
err_creation_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    kedr_coi_payload_unregister(interceptor, &payload);
err_payload:
    return result;
}
