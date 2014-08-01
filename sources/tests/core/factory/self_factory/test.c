/*
 * Use factory interceptor for same operations as binded one.
 */

#include <kedr-coi/operations_interception.h>

#define OPERATION_OFFSET(op_name) offsetof(struct test_operations, op_name)
#include "test_harness.h"
#include "test_harness_factory.h"

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
KEDR_COI_TEST_DEFINE_HANDLER_FUNC(clone_post, clone_post_call_counter)

int op_pre_call_counter;
KEDR_COI_TEST_DEFINE_HANDLER_FUNC(op_pre, op_pre_call_counter)

static struct kedr_coi_handler pre_handlers[] =
{
    HANDLER(op, op_pre),
    kedr_coi_handler_end
};

static struct kedr_coi_handler post_handlers[] =
{
    HANDLER(clone, clone_post),
    kedr_coi_handler_end
};

static struct kedr_coi_payload payload =
{
    .pre_handlers = pre_handlers,
    .post_handlers = post_handlers
};

// Prototype object has same type as normal
static void* get_factory(void* data)
{
    return data;
}
// Factory interceptor
struct kedr_coi_factory_interceptor* factory_interceptor;

KEDR_COI_TEST_DEFINE_FACTORY_INTERMEDIATE_FUNC(clone_factory_repl,
    get_factory, OPERATION_OFFSET(clone), factory_interceptor);

static struct kedr_coi_intermediate factory_intermediate_operations[] =
{
    INTERMEDIATE(clone, clone_factory_repl),
    INTERMEDIATE_FINAL
};


//******************Test infrastructure**********************************//
int test_init(void)
{
    interceptor = INDIRECT_CONSTRUCTOR("Indirect interceptor with self-factory support",
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
        "Factory interceptor with self-factory support",
        offsetof(struct test_object, ops),
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
    struct test_object factory = {.ops = &test_operations_orig};
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
    
    result = kedr_coi_interceptor_watch(interceptor,
        &factory);
    if(result < 0)
    {
        pr_err("Interceptor failed to watch for an object.");
        goto err_watch;
    }
    
    result = kedr_coi_factory_interceptor_watch(factory_interceptor,
        &factory);
    if(result < 0)
    {
        pr_err("Factory interceptor failed to watch for an object(self-factory).");
        goto err_factory_watch;
    }

    // Check that normal interception functionality is not lost.
    op_call_counter = 0;
    op_pre_call_counter = 0;
    
    factory.ops->op(&factory, NULL);
    
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



    // As if another object was created from prototype and its 'clone'
    // operation was called.
    object.ops = factory.ops;

    clone_call_counter = 0;
    clone_post_call_counter = 0;
    
    object.ops->clone(&object, &factory);
    
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

    kedr_coi_factory_interceptor_forget(factory_interceptor, &factory);
    kedr_coi_interceptor_forget(interceptor, &factory);
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor, &payload);

    return 0;

err_test:
    kedr_coi_interceptor_forget(interceptor, &object);
err_forget:
    kedr_coi_factory_interceptor_forget(factory_interceptor, &factory);
err_factory_watch:
    kedr_coi_interceptor_forget(interceptor, &factory);
err_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    kedr_coi_payload_unregister(interceptor, &payload);
err_payload:
    return result;
}
