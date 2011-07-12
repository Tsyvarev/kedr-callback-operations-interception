/*
 * Test that indirect instrumentor correctly works with empty replacements.
 */

#include "kedr_coi_instrumentor_internal.h"
#include "kedr_coi_global_map_internal.h"

#include "instrumentor_test_common.h"

/* Operations for test */
struct test_operations
{
    void* some_field;
    void (*do_something)(int value);
    void* other_fields[5];
    void (*unchanged_action)(int value);
};
/* Object for test */
struct test_object
{
    int some_fields[10];
    const struct test_operations* ops;
};

/* Original operation */
static void do_something_orig(int value)
{
}

/* Replacement operation */
static void do_something_repl(int value)
{
}

/* Operations which shouldn't be replaced */
static void unchanged_action(int value)
{
}

static const struct test_operations ops =
{
    .do_something = &do_something_orig,
    .unchanged_action = unchanged_action,
};

struct test_object object =
{
    .ops = &ops,
};

static struct kedr_coi_instrumentor_replacement replacements[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something),
        .repl = (void*)&do_something_repl
    },
    {
        .operation_offset = -1
    }
};

static struct kedr_coi_test_replaced_operation replaced_operations[] =
{
    {
        .offset = offsetof(struct test_operations, do_something),
        .orig = (void*)&do_something_orig,
        .repl = (void*)&do_something_repl
    },
    {
        .offset = -1
    }
};

static struct kedr_coi_test_unaffected_operation unaffected_operations[] =
{
    {
        .offset = offsetof(struct test_operations, unchanged_action),
        .orig = (void*)&unchanged_action,
    },
    {
        .offset = -1
    }
};

/********************Testing infrastructure***************************/
// Initialization/cleaning
int test_init(void)
{
    int result;
    
    result = kedr_coi_global_map_init();
    if(result)
    {
        pr_err("Failed to init global map.");
        return result;
    }
    
    return 0;
}

void test_cleanup(void)
{
    kedr_coi_global_map_destroy();
}

// Test itself
int test_run(void)
{
    int result;
    
    struct kedr_coi_instrumentor* instrumentor =
        kedr_coi_instrumentor_create_indirect(
            offsetof(struct test_object, ops),
            sizeof(struct test_operations),
            replacements);
    
    if(instrumentor == NULL)
    {
        pr_err("Failed to create indirect instrumentor.");
        return -1;
    }
    
    result = kedr_coi_instrumentor_watch(instrumentor, &object);
    if(result < 0)
    {
        pr_err("Fail to watch for an object.");
        kedr_coi_instrumentor_destroy(instrumentor);
        return result;
    }
    
    result = check_object_instrumented(&object,
        instrumentor,
        object.ops,
        replaced_operations,
        unaffected_operations);
    if(result)
    {
        pr_err("Incorrect object instrumentation.");
        kedr_coi_instrumentor_forget(instrumentor, &object, 0);
        kedr_coi_instrumentor_destroy(instrumentor);
        return result;
    }
    
    kedr_coi_instrumentor_forget(instrumentor, &object, 0);
    if(result < 0)
    {
        pr_err("Fail to forget object.");
        kedr_coi_instrumentor_destroy(instrumentor);
        return -1;
    }

    result = check_object_uninstrumented(&object,
        object.ops,
        replaced_operations,
        unaffected_operations);

    if(result)
    {
        pr_err("Incorrect restoring instrumentation of object.");
        kedr_coi_instrumentor_destroy(instrumentor);
        return result;
    }
    
    kedr_coi_instrumentor_destroy(instrumentor);
    return 0;
}
