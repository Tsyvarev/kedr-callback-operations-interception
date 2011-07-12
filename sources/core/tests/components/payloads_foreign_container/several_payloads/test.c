/*
 * Test functuonality of container for several foreign payloads.
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
    void* other_fields[5];
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


/* on_create_handler for first payload*/
static void on_create_handler1(void* object)
{
}

static kedr_coi_handler_foreign_t on_create_handlers1[] =
{
    &on_create_handler1,
    NULL
};

// First payload
static struct kedr_coi_payload_foreign payload1 =
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


static kedr_coi_handler_foreign_t on_create_handlers2[] =
{
    &on_create_handler2_1,
    &on_create_handler2_2,
    NULL
};

// Second payload
static struct kedr_coi_payload_foreign payload2 =
{
    .on_create_handlers = on_create_handlers2
};

static kedr_coi_handler_foreign_t on_create_handlers_all[] =
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
    
    result = kedr_coi_payloads_foreign_container_register_payload(payload_container,
        &payload1);
    
    if(result)
    {
        pr_err("Failed to register payload by the container.");
        kedr_coi_payloads_foreign_container_destroy(payload_container,
            interceptor_name);
        return result;
    }
    
    result = kedr_coi_payloads_foreign_container_register_payload(payload_container,
        &payload2);
    
    if(result)
    {
        pr_err("Failed to register another payload by the container.");
        kedr_coi_payloads_foreign_container_unregister_payload(payload_container,
            &payload1);
        kedr_coi_payloads_foreign_container_destroy(payload_container,
            interceptor_name);
        return result;
    }


    replacements =
        kedr_coi_payloads_foreign_container_fix_payloads(payload_container);
    if(IS_ERR(replacements))
    {
        pr_err("Failed to fix payloads in the container.");
        kedr_coi_payloads_foreign_container_unregister_payload(payload_container,
            &payload1);
        kedr_coi_payloads_foreign_container_unregister_payload(payload_container,
            &payload2);
        kedr_coi_payloads_foreign_container_destroy(payload_container,
            interceptor_name);
        return PTR_ERR(replacements);
    }
    
    if(replacements == NULL)
    {
        pr_err("Unexpected NULL array as replacements.");
        kedr_coi_payloads_foreign_container_unregister_payload(payload_container,
            &payload1);
        kedr_coi_payloads_foreign_container_unregister_payload(payload_container,
            &payload2);
        kedr_coi_payloads_foreign_container_destroy(payload_container,
            interceptor_name);
        return -1;
    }
    
    result = check_foreign_replacements(replacements,
        &intermediate_info,
        intermediate_operations,
        on_create_handlers_all);

    kedr_coi_payloads_foreign_container_release_payloads(payload_container);
    kedr_coi_payloads_foreign_container_unregister_payload(payload_container,
        &payload1);
    kedr_coi_payloads_foreign_container_unregister_payload(payload_container,
        &payload2);
    kedr_coi_payloads_foreign_container_destroy(payload_container,
        interceptor_name);

    if(result)
    {
        pr_err("kedr_coi_payloads_foreign_container_fix_payloads() returns incorrect replacements.");
        return result;
    }

    return 0;
}