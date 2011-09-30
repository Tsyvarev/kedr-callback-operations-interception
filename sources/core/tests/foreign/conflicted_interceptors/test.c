/*
 * Test functionality of conflicted foreign interceptors.
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


// Another foreign interceptor for same prototype object
struct kedr_coi_foreign_interceptor* foreign_interceptor_another;

KEDR_COI_TEST_DEFINE_FOREIGN_INTERMEDIATE_FUNC(op1_foreign_repl_another,
    get_prototype_object, OPERATION_OFFSET(op1), foreign_interceptor_another);

static struct kedr_coi_foreign_intermediate foreign_intermediate_operations_another[] =
{
    INTERMEDIATE(op1, op1_foreign_repl_another),
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

    foreign_interceptor_another = kedr_coi_foreign_interceptor_create(
        interceptor,
        "Another simple foreign interceptor",
        offsetof(struct test_prototype_object, foreign_ops),
        foreign_intermediate_operations_another,
        NULL);
    
    if(foreign_interceptor_another == NULL)
    {
        pr_err("Failed to create another foreign interceptor for test.");
        kedr_coi_foreign_interceptor_destroy(foreign_interceptor);
        kedr_coi_interceptor_destroy(interceptor);
        return -EINVAL;
    }



    return 0;
}
void test_cleanup(void)
{
    kedr_coi_foreign_interceptor_destroy(foreign_interceptor_another);
    kedr_coi_foreign_interceptor_destroy(foreign_interceptor);
    kedr_coi_interceptor_destroy(interceptor);
}

// Test itself
int test_run(void)
{
    int result;
    struct test_prototype_object prototype_object =
        {.foreign_ops = &test_operations_orig};
    
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
    
    result = kedr_coi_foreign_interceptor_watch(foreign_interceptor,
        &prototype_object);
    if(result < 0)
    {
        pr_err("Foreign interceptor failed to watch for an object.");
        goto err_foreign_watch;
    }

    result = kedr_coi_foreign_interceptor_watch(foreign_interceptor_another,
        &prototype_object);
    if(result >= 0)
    {
        pr_err("Another foreign interceptor succeed to watch for the "
            "same object. It is an error");
        kedr_coi_foreign_interceptor_forget(foreign_interceptor_another,
            &prototype_object);
        result = -EINVAL;
        goto err_test;
    }

    kedr_coi_foreign_interceptor_forget(foreign_interceptor, &prototype_object);
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor, &payload);

    return 0;

err_test:
    kedr_coi_foreign_interceptor_forget(foreign_interceptor, &prototype_object);
err_foreign_watch:
    kedr_coi_interceptor_stop(interceptor);
err_start:
    kedr_coi_payload_unregister(interceptor, &payload);
err_payload:
    return result;
}