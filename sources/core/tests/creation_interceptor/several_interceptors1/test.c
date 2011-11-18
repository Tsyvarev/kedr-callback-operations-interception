/*
 * Test functionality of several creation interceptors with same operations.
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

// Creation interceptor
static void* get_tie(void* data)
{
    return data;
}

struct kedr_coi_creation_interceptor* creation_interceptor;

KEDR_COI_TEST_DEFINE_CREATION_INTERMEDIATE_FUNC(op1_creation_repl,
    get_tie, OPERATION_OFFSET(op1), creation_interceptor);

static struct kedr_coi_creation_intermediate creation_intermediate_operations[] =
{
    INTERMEDIATE(op1, op1_creation_repl),
    INTERMEDIATE_FINAL
};


// Another creation interceptor
static void* get_tie_another(void* data)
{
    return data;
}

struct kedr_coi_creation_interceptor* creation_interceptor_another;

KEDR_COI_TEST_DEFINE_CREATION_INTERMEDIATE_FUNC(op1_creation_repl_another,
    get_tie_another, OPERATION_OFFSET(op1), creation_interceptor_another);

static struct kedr_coi_creation_intermediate creation_intermediate_operations_another[] =
{
    INTERMEDIATE(op1, op1_creation_repl_another),
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
    
    creation_interceptor = kedr_coi_creation_interceptor_create(
        interceptor,
        "Simple creation interceptor",
        creation_intermediate_operations,
        NULL);
    
    if(creation_interceptor == NULL)
    {
        pr_err("Failed to create creation interceptor for test.");
        kedr_coi_interceptor_destroy(interceptor);
        return -EINVAL;
    }

    creation_interceptor_another = kedr_coi_creation_interceptor_create(
        interceptor,
        "Another simple creation interceptor",
        creation_intermediate_operations_another,
        NULL);
    
    if(creation_interceptor_another == NULL)
    {
        pr_err("Failed to create another creation interceptor for test.");
        kedr_coi_creation_interceptor_destroy(creation_interceptor);
        kedr_coi_interceptor_destroy(interceptor);
        return -EINVAL;
    }



    return 0;
}
void test_cleanup(void)
{
    kedr_coi_creation_interceptor_destroy(creation_interceptor_another);
    kedr_coi_creation_interceptor_destroy(creation_interceptor);
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
        
    const struct test_operations* object_operations_another =
        &test_operations_orig;
    void* id_another = (void*)0x124;
    void* tie_another = (void*)0x653;
    
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
    
    result = kedr_coi_creation_interceptor_watch(creation_interceptor,
        id, tie, (const void**)&object_operations);
    if(result < 0)
    {
        pr_err("Creation interceptor failed to watch operations.");
        goto err_creation_watch;
    }

    result = kedr_coi_creation_interceptor_watch(creation_interceptor_another,
        id_another, tie_another, (const void**)&object_operations_another);
    if(result < 0)
    {
        pr_err("Another creation interceptor failed to watch operations "
            "which has same content as another watched operations.");
        goto err_creation_watch_another;
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

    // As if normal object was created from another watched operations
    // and its operation 1 was called.
    object.ops = object_operations_another;

    op1_call_counter = 0;
    op1_pre1_call_counter = 0;
    
    object.ops->op1(&object, tie_another);
    
    if(op1_call_counter == 0)
    {
        pr_err("Original operation 1 wasn't called "
            "when use another watched operations.");
        result = -EINVAL;
        goto err_test;
    }

    if(op1_pre1_call_counter == 0)
    {
        pr_err("Pre handler for operation 1 wasn't called "
            "when use another watched operations.");
        result = -EINVAL;
        goto err_test;
    }

    // Check another operation
    op2_call_counter = 0;
    op2_post1_call_counter = 0;
    
    object.ops->op2(&object, NULL);
    
    if(op2_post1_call_counter == 0)
    {
        pr_err("Post handler for operation 2 wasn't called "
            "when use another watched operations.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op2_call_counter == 0)
    {
        pr_err("Original operation 2 wasn't called "
            "when use another watched operations.");
        result = -EINVAL;
        goto err_test;
    }

    result = kedr_coi_interceptor_forget(interceptor, &object);
    if(result < 0)
    {
        pr_err("Error occured when forget normal object "
            "used another watched operations.");
        goto err_forget;
    }
    if(result == 1)
    {
        pr_err("Normal object should be automatically watched, but 'forget' return 1(another prototype).");
        result = -EINVAL;
        goto err_forget;
    }

    kedr_coi_creation_interceptor_forget(creation_interceptor_another,
        id_another, (const void**)&object_operations_another);
    kedr_coi_creation_interceptor_forget(creation_interceptor,
        id, (const void**)&object_operations);
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor, &payload);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object);
err_forget:
    kedr_coi_creation_interceptor_forget(creation_interceptor_another,
        id_another, (const void**)&object_operations_another);
err_creation_watch_another:
    kedr_coi_creation_interceptor_forget(creation_interceptor,
        id, (const void**)&object_operations);
err_creation_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    kedr_coi_payload_unregister(interceptor, &payload);
err_payload:
    return result;
}