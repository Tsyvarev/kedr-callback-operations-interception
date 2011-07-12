/*
 * Module for test that indirect foreign instrumentor instruments operations correctly.
 */

#include <linux/module.h>

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

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
    const struct test_operations* foreign_ops;
};

/* Foreign object for test*/
struct test_foreign_object
{
    int some_fields[3];
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

static struct test_object object =
{
    .foreign_ops = &ops,
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
//Initialize/cleaning
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
    struct test_foreign_object foreign_object;
    
    struct kedr_coi_instrumentor* instrumentor =
        kedr_coi_instrumentor_create_indirect_with_foreign(
            offsetof(struct test_object, foreign_ops),
            sizeof(struct test_operations),
            offsetof(struct test_foreign_object, ops),
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
        return -1;
    }
    
    result = check_object_instrumented_foreign(&object,
        instrumentor,
        object.foreign_ops,
        replaced_operations,
        unaffected_operations);
    if(result)
    {
        pr_err("Incorrect object instrumentation.");
        kedr_coi_instrumentor_forget(instrumentor, &object, 0);
        kedr_coi_instrumentor_destroy(instrumentor);
        return result;
    }
    
    // As if foreign object is created with operations copied from initial object.
    foreign_object.ops = object.foreign_ops;
    
    result = kedr_coi_instrumentor_foreign_restore_copy(instrumentor,
        &object, &foreign_object);
    if(result < 0)
    {
        pr_err("Fail to restore operations in foreign object.");
        kedr_coi_instrumentor_forget(instrumentor, &object, 0);
        kedr_coi_instrumentor_destroy(instrumentor);
        return -1;
    }
    
    result = check_object_foreign_restored(&foreign_object,
        foreign_object.ops,
        replaced_operations,
        unaffected_operations);
    if(result)
    {
        pr_err("Operations of foreign object was restored incorrectly.");
        kedr_coi_instrumentor_forget(instrumentor, &object, 0);
        kedr_coi_instrumentor_destroy(instrumentor);
        return result;
    }
    
    
    result = kedr_coi_instrumentor_forget(instrumentor, &object, 0);
    if(result < 0)
    {
        pr_err("Fail to forget object.");
        kedr_coi_instrumentor_destroy(instrumentor);
        return -1;
    }

    result = check_object_uninstrumented(&object,
        object.foreign_ops,
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

