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
static struct kedr_coi_intermediate_info do_something1_intermediate_info;
static void do_something1_repl(int value)
{
}

/* Intermediate operation2 */
static struct kedr_coi_intermediate_info do_something2_intermediate_info;
static void do_something2_repl(int value)
{
}

/* Intermediate operation3 */
static struct kedr_coi_intermediate_info do_something3_intermediate_info;
static void do_something3_repl(int value)
{
}


static struct kedr_coi_intermediate intermediate_operations[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .repl = (void*)&do_something1_repl,
        .info = &do_something1_intermediate_info,
        .group_id = 10
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something2),
        .repl = (void*)&do_something2_repl,
        .info = &do_something2_intermediate_info
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something3),
        .repl = (void*)&do_something3_repl,
        .info = &do_something3_intermediate_info,
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

static struct kedr_coi_handler_pre handlers_pre[] =
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
    .handlers_pre = handlers_pre,
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
    struct kedr_coi_instrumentor_replacement* replacements;
    
    struct kedr_coi_payloads_container* payload_container =
        kedr_coi_payloads_container_create(intermediate_operations);
    
    if(payload_container == NULL)
    {
        pr_err("Failed to create payload container.");
        return -1;
    }
    
    result = kedr_coi_payloads_container_register_payload(payload_container,
        &payload);
    
    if(result)
    {
        pr_err("Failed to register payload by the container.");
        kedr_coi_payloads_container_destroy(payload_container,
            interceptor_name);
        return result;
    }
    
    replacements =
        kedr_coi_payloads_container_fix_payloads(payload_container);
    if(IS_ERR(replacements))
    {
        pr_err("Failed to fix payloads in the container.");
        kedr_coi_payloads_container_unregister_payload(payload_container,
            &payload);
        kedr_coi_payloads_container_destroy(payload_container,
            interceptor_name);
        return PTR_ERR(replacements);
    }
    
    if(replacements == NULL)
    {
        pr_err("Unexpected NULL array as replacements.");
        kedr_coi_payloads_container_unregister_payload(payload_container,
            &payload);
        kedr_coi_payloads_container_destroy(payload_container,
            interceptor_name);
        return -1;
    }
    
    result = check_replacements(replacements, replacements_expected);
    //result = 0;
    
    kedr_coi_payloads_container_release_payloads(payload_container);
    kedr_coi_payloads_container_unregister_payload(payload_container,
        &payload);
    kedr_coi_payloads_container_destroy(payload_container,
        interceptor_name);

    if(result)
    {
        pr_err("kedr_coi_payloads_container_fix_payloads() returns incorrect replacements.");
        return result;
    }
    
    return 0;
}