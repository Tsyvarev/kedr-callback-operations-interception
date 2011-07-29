/*
 * Test functuonality of payloads container with empty payload(no handlers).
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
};

/* Intermediate operation1 */
static void do_something1_repl(int value)
{
}

/* Intermediate operation2 */
static void do_something2_repl(int value)
{
}

static struct kedr_coi_intermediate intermediate_operations[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .repl = (void*)&do_something1_repl,
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something2),
        .repl = (void*)&do_something2_repl,
    },
    {
        .operation_offset = -1
    }
};


static struct kedr_coi_payload payload =
{
};


static struct kedr_coi_test_replacement replacements_expected[] =
{
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