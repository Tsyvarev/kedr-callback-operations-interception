/*
 * Test functuonality of grouping intermediates in the payloads container.
 */

#include "kedr_coi_payloads_internal.h"

#include "payloads_container_test_common.h"

// Need for call kedr_coi_payloads_container_destroy()
const char* interceptor_name = "test_interceptor";

/* Operations for test */
struct test_operations
{
    void* some_field;
    void (*do_something1)(int value);
    void* other_fields[5];
    void (*do_something2)(int value);
    void (*do_something3)(int value);
};

/* Intermediate operation1 */
static void do_something1_repl(int value)
{
}

/* Intermediate operation2 */
static void do_something2_repl(int value)
{
}

/* Intermediate operation3 */
static void do_something3_repl(int value)
{
}


static struct kedr_coi_intermediate intermediate_operations[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .repl = (void*)&do_something1_repl,
        .group_id = 10
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something2),
        .repl = (void*)&do_something2_repl,
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something3),
        .repl = (void*)&do_something3_repl,
        .group_id = 10
    },

    {
        .operation_offset = -1
    }
};


/* handler of the operation1 */
static void do_something1_pre1(int value,
    struct kedr_coi_operation_call_info* call_info)
{
}

static struct kedr_coi_pre_handler pre_handlers[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .func = (void*)&do_something1_pre1
    },
    {
        .operation_offset = -1
    }
};

static struct kedr_coi_payload payload =
{
    .pre_handlers = pre_handlers,
};


// All functions which should be called before operation1
static void* do_something1_pre[] =
{
    (void*)&do_something1_pre1,
    NULL
};

static struct kedr_coi_test_replacement replacements_expected[] =
{
    {
        .intermediate = &intermediate_operations[0],
        .pre = do_something1_pre,
        .post = NULL
    },
    // Should be included only because of grouping
    {
        .intermediate = &intermediate_operations[2],
        .pre = NULL,
        .post = NULL
    },
    {
        .intermediate = NULL
    }
};

//******************Test infrastructure**********************************//
int test_init(void)
{
    return 0;
}
void test_cleanup(void)
{
}

// Test itself
int test_run(void)
{
    int result;
    
    struct kedr_coi_payloads_container* payloads_container =
        kedr_coi_payloads_container_create(intermediate_operations);
    
    if(payloads_container == NULL)
    {
        pr_err("Failed to create payload container.");
        return -1;
    }
    
    result = kedr_coi_payloads_container_register_payload(payloads_container,
        &payload);
    
    if(result)
    {
        pr_err("Failed to register payload by the container.");
        kedr_coi_payloads_container_destroy(payloads_container,
            interceptor_name);
        return result;
    }
    
    result = kedr_coi_payloads_container_fix_payloads(payloads_container);
    if(result)
    {
        pr_err("Failed to fix payloads in the container.");
        kedr_coi_payloads_container_unregister_payload(payloads_container,
            &payload);
        kedr_coi_payloads_container_destroy(payloads_container,
            interceptor_name);
        return result;
    }
    
    result = check_payloads_container(payloads_container, replacements_expected);
    
    kedr_coi_payloads_container_release_payloads(payloads_container);
    kedr_coi_payloads_container_unregister_payload(payloads_container,
        &payload);
    kedr_coi_payloads_container_destroy(payloads_container,
        interceptor_name);

    if(result)
    {
        pr_err("Payloads container gets incorrect interception information.");
        return result;
    }
    
    return 0;
}