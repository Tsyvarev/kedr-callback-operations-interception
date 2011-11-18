/*
 * Use creation interceptor for same operations as binded one.
 */

#include <kedr-coi/operations_interception.h>

#define OPERATION_OFFSET(op_name) offsetof(struct test_operations, op_name)
#include "test_harness.h"
#include "test_harness_creation.h"

/* Operations for test */
struct test_operations
{
    void* some_field;
    kedr_coi_test_op_t clone;
    void* other_fields[5];
    kedr_coi_test_op_t op;
};

struct test_object
{
    int some_fields[3];
    const struct test_operations* ops;
};

int clone_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(clone_orig, clone_call_counter);

int op_call_counter = 0;
KEDR_COI_TEST_DEFINE_OP_ORIG(op_orig, op_call_counter);

struct test_operations test_operations_orig =
{
    .clone = clone_orig,
    .op = op_orig
};

struct kedr_coi_interceptor* interceptor;

KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(clone_repl, OPERATION_OFFSET(clone), interceptor);
KEDR_COI_TEST_DEFINE_INTERMEDIATE_FUNC(op_repl, OPERATION_OFFSET(op), interceptor);

static struct kedr_coi_intermediate intermediate_operations[] =
{
    INTERMEDIATE(clone, clone_repl),
    INTERMEDIATE(op, op_repl),
    INTERMEDIATE_FINAL
};


int clone_post_call_counter;
KEDR_COI_TEST_DEFINE_POST_HANDLER_FUNC(clone_post, clone_post_call_counter)

int op_pre_call_counter;
KEDR_COI_TEST_DEFINE_PRE_HANDLER_FUNC(op_pre, op_pre_call_counter)

static struct kedr_coi_pre_handler pre_handlers[] =
{
    PRE_HANDLER(op, op_pre),
    kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler post_handlers[] =
{
    POST_HANDLER(clone, clone_post),
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

KEDR_COI_TEST_DEFINE_CREATION_INTERMEDIATE_FUNC(clone_creation_repl,
    get_tie, OPERATION_OFFSET(clone), creation_interceptor);

static struct kedr_coi_creation_intermediate creation_intermediate_operations[] =
{
    INTERMEDIATE(clone, clone_creation_repl),
    INTERMEDIATE_FINAL
};


//******************Test infrastructure**********************************//
int test_init(void)
{
    interceptor = INDIRECT_CONSTRUCTOR("Indirect interceptor with self-creation support",
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
        "Creation interceptor with self-creation support",
        creation_intermediate_operations,
        NULL);
    
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
    kedr_coi_creation_interceptor_destroy(creation_interceptor);
    kedr_coi_interceptor_destroy(interceptor);
}

// Test itself
int test_run(void)
{
    int result;

    struct test_object object_creator = {.ops = &test_operations_orig};
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
    
    result = kedr_coi_interceptor_watch(interceptor, &object_creator);
    if(result < 0)
    {
        pr_err("Interceptor failed to watch for an object.");
        goto err_watch;
    }
    
    result = kedr_coi_creation_interceptor_watch(creation_interceptor,
        &object_creator, tie, (const void**)&object_creator.ops);
    if(result < 0)
    {
        pr_err("Creation interceptor failed to watch for an object(self-creation).");
        goto err_creation_watch;
    }

    // Check that normal interception functionality is not lost.
    op_call_counter = 0;
    op_pre_call_counter = 0;
    
    object_creator.ops->op(&object_creator, tie);
    
    if(op_pre_call_counter == 0)
    {
        pr_err("Pre handler for operation wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op_call_counter == 0)
    {
        pr_err("Original operation wasn't called.");
        result = -EINVAL;
        goto err_test;
    }


    // As if another object was created from creator and its 'clone'
    // operation was called.
    object.ops = object_creator.ops;

    clone_call_counter = 0;
    clone_post_call_counter = 0;
    
    object.ops->clone(&object, tie);
    
    if(clone_post_call_counter == 0)
    {
        pr_err("Post handler for 'clone' operation wasn't called.");
        result = -EINVAL;
        goto err_test;
    }

    if(clone_call_counter == 0)
    {
        pr_err("Original 'clone' operation wasn't called.");
        result = -EINVAL;
        goto err_test;
    }

    // Check another operation
    op_call_counter = 0;
    op_pre_call_counter = 0;
    
    object.ops->op(&object, NULL);
    
    if(op_pre_call_counter == 0)
    {
        pr_err("Pre handler for operation wasn't called.");
        result = -EINVAL;
        goto err_test;
    }
    
    if(op_call_counter == 0)
    {
        pr_err("Original operation wasn't called.");
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

    kedr_coi_creation_interceptor_forget(creation_interceptor,
        &object_creator, (const void**)&object_creator.ops);
    kedr_coi_interceptor_forget(interceptor, &object_creator);
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor, &payload);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object);
err_forget:
    kedr_coi_creation_interceptor_forget(creation_interceptor,
        &object_creator, (const void**)&object_creator.ops);
err_creation_watch:
    kedr_coi_interceptor_forget(interceptor, &object_creator);
err_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    kedr_coi_payload_unregister(interceptor, &payload);
err_payload:
    return result;
}