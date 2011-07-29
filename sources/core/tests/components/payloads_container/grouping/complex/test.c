/*
 * Test base grouping functuonality of payloads container in complex case.
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
    void* another_fields[7];
    void (*do_something3)(int value);
    void (*do_something4)(int value);
    void (*do_something5)(int value);
    void (*do_something6)(int value);
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

/* Intermediate operation4 */
static void do_something4_repl(int value)
{
}

/* Intermediate operation5 */
static void do_something5_repl(int value)
{
}

/* Intermediate operation6 */
static void do_something6_repl(int value)
{
}


static struct kedr_coi_intermediate intermediate_operations[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .repl = (void*)&do_something1_repl,
        .group_id = 1
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something2),
        .repl = (void*)&do_something2_repl,
        .group_id = 1
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something3),
        .repl = (void*)&do_something3_repl,
        .group_id = 2
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something4),
        .repl = (void*)&do_something4_repl,
        .group_id = 2
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something5),
        .repl = (void*)&do_something5_repl,
        .group_id = 3
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something6),
        .repl = (void*)&do_something6_repl,
        .group_id = 3
    },

    {
        .operation_offset = -1
    }
};


/* handler of the operation1 of first payload*/
static void do_something1_pre1(int value,
    struct kedr_coi_operation_call_info* call_info)
{
}

/* handler of the operation4 of first payload*/
static void do_something4_post1(int value,
    struct kedr_coi_operation_call_info* call_info)
{
}


// All handlers of the first payload
static struct kedr_coi_pre_handler pre_handlers1[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .func = (void*)&do_something1_pre1
    },
    {
        .operation_offset = -1
    }
};

static struct kedr_coi_post_handler post_handlers1[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something4),
        .func = (void*)&do_something4_post1
    },
    {
        .operation_offset = -1
    }
};

// First payload
static struct kedr_coi_payload payload1 =
{
    .pre_handlers = pre_handlers1,
    .post_handlers = post_handlers1,
};



/* handler of the operation1 of second payload*/
static void do_something1_pre2(int value,
    struct kedr_coi_operation_call_info* call_info)
{
}


/* handler of the operation3 of second payload*/
static void do_something3_post2(int value,
    struct kedr_coi_operation_call_info* call_info)
{
}

/* another handler of the operation3 of second payload*/
static void do_something3_post2_1(int value,
    struct kedr_coi_operation_call_info* call_info)
{
}

/* some another handler of the operation3 of second payload*/
static void do_something3_pre2(int value,
    struct kedr_coi_operation_call_info* call_info)
{
}

// All handlers of the second payload
static struct kedr_coi_pre_handler pre_handlers2[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .func = (void*)&do_something1_pre2
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something3),
        .func = (void*)&do_something3_pre2
    },
    {
        .operation_offset = -1
    }
};


static struct kedr_coi_post_handler post_handlers2[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something3),
        .func = (void*)&do_something3_post2
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something3),
        .func = (void*)&do_something3_post2_1
    },
    {
        .operation_offset = -1
    }
};

// Second payload
static struct kedr_coi_payload payload2 =
{
    .pre_handlers = pre_handlers2,
    .post_handlers = post_handlers2
};

// All functions which should be called before operation1
static void* do_something1_pre[] =
{
    (void*)&do_something1_pre1,
    (void*)&do_something1_pre2,
    NULL
};


// All functions which should be called before operation3
static void* do_something3_pre[] =
{
    (void*)&do_something3_pre2,
    NULL
};

// All functions which should be called after operation3
static void* do_something3_post[] =
{
    (void*)&do_something3_post2,
    (void*)&do_something3_post2_1,
    NULL
};

// All functions which should be called after operation4
static void* do_something4_post[] =
{
    (void*)&do_something4_post1,
    NULL
};


static struct kedr_coi_test_replacement replacements_expected[] =
{
    {
        .intermediate = &intermediate_operations[0],
        .pre = do_something1_pre,
        .post = NULL
    },
    {
        .intermediate = &intermediate_operations[2],
        .pre = do_something3_pre,
        .post = do_something3_post
    },
    // Should be added because of grouping, but it already has handlers
    {
        .intermediate = &intermediate_operations[3],
        .pre = NULL,
        .post = do_something4_post
    },
    // Should be added only because of grouping
    {
        .intermediate = &intermediate_operations[1],
        .pre = NULL,
        .post = NULL
    },
    // Intermediate for operations 5 and 6 shouldn't be added at all, even them in one group
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
        &payload1);
    
    if(result)
    {
        pr_err("Failed to register payload by the container.");
        kedr_coi_payloads_container_destroy(payloads_container,
            interceptor_name);
        return result;
    }
    
    result = kedr_coi_payloads_container_register_payload(payloads_container,
        &payload2);
    
    if(result)
    {
        pr_err("Failed to register another payload by the container.");
        kedr_coi_payloads_container_unregister_payload(payloads_container,
            &payload1);
        kedr_coi_payloads_container_destroy(payloads_container,
            interceptor_name);
        return result;
    }

    result = kedr_coi_payloads_container_fix_payloads(payloads_container);
    if(result)
    {
        pr_err("Failed to fix payloads in the container.");
        kedr_coi_payloads_container_unregister_payload(payloads_container,
            &payload2);
        kedr_coi_payloads_container_unregister_payload(payloads_container,
            &payload1);
        kedr_coi_payloads_container_destroy(payloads_container,
            interceptor_name);
        return result;
    }
    
    result = check_payloads_container(payloads_container, replacements_expected);
    
    kedr_coi_payloads_container_release_payloads(payloads_container);
    // Unregister payloads in same order - just for test
    kedr_coi_payloads_container_unregister_payload(payloads_container,
        &payload1);
    kedr_coi_payloads_container_unregister_payload(payloads_container,
        &payload2);
    kedr_coi_payloads_container_destroy(payloads_container,
        interceptor_name);

    if(result)
    {
        pr_err("Payloads container gets incorrect interception information.");
        return result;
    }
    
    return 0;
}