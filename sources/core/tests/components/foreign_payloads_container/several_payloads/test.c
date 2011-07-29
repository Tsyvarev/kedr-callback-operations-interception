/*
 * Test functuonality of container for several foreign payloads.
 */

#include "kedr_coi_payloads_internal.h"

#include "foreign_payloads_container_test_common.h"

// Need for call kedr_coi_foreign_payloads_container_destroy()
const char* interceptor_name = "test_interceptor";

/* Operations for test */
struct test_operations
{
    void* some_field;
    void (*do_something1)(int value);
    void* other_fields[5];
};

/* Intermediate operation1 */
static void do_something1_repl(int value)
{
}

static struct kedr_coi_foreign_intermediate intermediate_operations[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .repl = (void*)&do_something1_repl,
    },
    {
        .operation_offset = -1
    }
};


/* on_create_handler for first payload*/
static void on_create_handler1(void* object)
{
}

static kedr_coi_foreign_handler_t on_create_handlers1[] =
{
    &on_create_handler1,
    NULL
};

// First payload
static struct kedr_coi_foreign_payload payload1 =
{
    .on_create_handlers = on_create_handlers1
};


/* on_create_handlers for second payload*/
static void on_create_handler2_1(void* object)
{
}

static void on_create_handler2_2(void* object)
{
}


static kedr_coi_foreign_handler_t on_create_handlers2[] =
{
    &on_create_handler2_1,
    &on_create_handler2_2,
    NULL
};

// Second payload
static struct kedr_coi_foreign_payload payload2 =
{
    .on_create_handlers = on_create_handlers2
};

static kedr_coi_foreign_handler_t on_create_handlers_all[] =
{
    &on_create_handler1,
    &on_create_handler2_1,
    &on_create_handler2_2,
    NULL
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
    
    struct kedr_coi_foreign_payloads_container* payloads_container =
        kedr_coi_foreign_payloads_container_create(
            intermediate_operations);
    
    if(payloads_container == NULL)
    {
        pr_err("Failed to create container for foreign payloads.");
        return -1;
    }
    
    result = kedr_coi_foreign_payloads_container_register_payload(payloads_container,
        &payload1);
    
    if(result)
    {
        pr_err("Failed to register payload by the container.");
        kedr_coi_foreign_payloads_container_destroy(payloads_container,
            interceptor_name);
        return result;
    }
    
    result = kedr_coi_foreign_payloads_container_register_payload(payloads_container,
        &payload2);
    
    if(result)
    {
        pr_err("Failed to register another payload by the container.");
        kedr_coi_foreign_payloads_container_unregister_payload(payloads_container,
            &payload1);
        kedr_coi_foreign_payloads_container_destroy(payloads_container,
            interceptor_name);
        return result;
    }


    result = kedr_coi_foreign_payloads_container_fix_payloads(payloads_container);
    if(result)
    {
        pr_err("Failed to fix payloads in the container.");
        kedr_coi_foreign_payloads_container_unregister_payload(payloads_container,
            &payload1);
        kedr_coi_foreign_payloads_container_unregister_payload(payloads_container,
            &payload2);
        kedr_coi_foreign_payloads_container_destroy(payloads_container,
            interceptor_name);
        return result;
    }
    
    result = check_foreign_payloads_container(payloads_container,
        intermediate_operations,
        on_create_handlers_all);

    kedr_coi_foreign_payloads_container_release_payloads(payloads_container);
    kedr_coi_foreign_payloads_container_unregister_payload(payloads_container,
        &payload1);
    kedr_coi_foreign_payloads_container_unregister_payload(payloads_container,
        &payload2);
    kedr_coi_foreign_payloads_container_destroy(payloads_container,
        interceptor_name);

    if(result)
    {
        pr_err("Payloads container gets incorrect interception information.");
        return result;
    }

    return 0;
}