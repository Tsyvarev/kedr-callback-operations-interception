/*
 * Test functuonality of payloads container with no payloads.
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
static struct kedr_coi_intermediate_info do_something1_intermediate_info;
static void do_something1_repl(int value)
{
}

/* Intermediate operation2 */
static struct kedr_coi_intermediate_info do_something2_intermediate_info;
static void do_something2_repl(int value)
{
}

static struct kedr_coi_intermediate intermediate_operations[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .repl = (void*)&do_something1_repl,
        .info = &do_something1_intermediate_info
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something2),
        .repl = (void*)&do_something2_repl,
        .info = &do_something2_intermediate_info
    },
    {
        .operation_offset = -1
    }
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
    struct kedr_coi_instrumentor_replacement* replacements;
    
    struct kedr_coi_payloads_container* payload_container =
        kedr_coi_payloads_container_create(intermediate_operations);
    
    if(payload_container == NULL)
    {
        pr_err("Failed to create payload container.");
        return -1;
    }
    
    replacements =
        kedr_coi_payloads_container_fix_payloads(payload_container);
    if(IS_ERR(replacements))
    {
        pr_err("Failed to fix payloads in the container with no payloads.");
        kedr_coi_payloads_container_destroy(payload_container,
            interceptor_name);
        return PTR_ERR(replacements);
    }
    
    if(replacements == NULL)
    {
        pr_err("Unexpected NULL array as replacements.");
        kedr_coi_payloads_container_destroy(payload_container,
            interceptor_name);
        return -1;
    }
    
    result = check_replacements(replacements, replacements_expected);
    
    kedr_coi_payloads_container_release_payloads(payload_container);
    kedr_coi_payloads_container_destroy(payload_container,
        interceptor_name);

    if(result)
    {
        pr_err("kedr_coi_payloads_container_fix_payloads() returns incorrect replacements.");
        return result;
    }
    
    return 0;
}