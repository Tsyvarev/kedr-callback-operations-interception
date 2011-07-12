/*
 * Test functuonality of container for foreign payloads with no payloads.
 */

#include "kedr_coi_payloads_foreign_internal.h"

#include "payloads_foreign_container_test_common.h"

// Need for call kedr_coi_payloads_foreign_container_destroy()
const char* interceptor_name = "test_interceptor";

/* Operations for test */
struct test_operations
{
    void* some_field;
    void (*do_something1)(int value);
};

static struct kedr_coi_intermediate_foreign_info intermediate_info;

/* Intermediate operation1 */

static void do_something1_repl(int value)
{
}


static struct kedr_coi_intermediate_foreign intermediate_operations[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .repl = (void*)&do_something1_repl,
    },
    {
        .operation_offset = -1
    }
};


// With no handlers no replacements should be made
static struct kedr_coi_intermediate_foreign* intermediate_operations_no =
    intermediate_operations + ARRAY_SIZE(intermediate_operations) - 1;
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
    
    struct kedr_coi_payloads_foreign_container* payload_container =
        kedr_coi_payloads_foreign_container_create(
            intermediate_operations,
            &intermediate_info);
    
    if(payload_container == NULL)
    {
        pr_err("Failed to create container for foreign payloads.");
        return -1;
    }
    
    replacements =
        kedr_coi_payloads_foreign_container_fix_payloads(payload_container);
    if(IS_ERR(replacements))
    {
        pr_err("Failed to fix payloads in the container.");
        kedr_coi_payloads_foreign_container_destroy(payload_container,
            interceptor_name);
        return PTR_ERR(replacements);
    }
    
    if(replacements == NULL)
    {
        pr_err("Unexpected NULL array as replacements.");
        kedr_coi_payloads_foreign_container_destroy(payload_container,
            interceptor_name);
        return -1;
    }
    
    result = check_foreign_replacements(replacements,
        &intermediate_info,
        intermediate_operations_no,
        NULL);

    kedr_coi_payloads_foreign_container_release_payloads(payload_container);
    kedr_coi_payloads_foreign_container_destroy(payload_container,
        interceptor_name);

    if(result)
    {
        pr_err("kedr_coi_payloads_foreign_container_fix_payloads() returns incorrect replacements.");
        return result;
    }

    return 0;
}