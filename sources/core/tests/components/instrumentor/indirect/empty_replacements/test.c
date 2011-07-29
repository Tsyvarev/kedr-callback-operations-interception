/*
 * Test that indirect instrumentor correctly works with empty replacements.
 */

#include "kedr_coi_instrumentor_internal.h"

#include <linux/kernel.h> /*printk*/
#include <linux/errno.h> /* error codes */

#include "instrumentor_test_common.h"

/* Operations for test */
struct test_operations
{
    void* some_field;
    void* other_fields[5];
    void (*unchanged_action)(int value);
};
/* Object for test */
struct test_object
{
    int some_fields[10];
    const struct test_operations* ops;
};


/* Operations which shouldn't be replaced */
static void unchanged_action(int value)
{
}

static const struct test_operations ops =
{
    .unchanged_action = unchanged_action,
};

struct test_object object =
{
    .ops = &ops,
};

static struct kedr_coi_replacement replacements[] =
{
    {
        .operation_offset = -1
    }
};

/********************Testing infrastructure***************************/
// Initialization/cleaning
int test_init(void)
{
    int result;
    
    result = kedr_coi_instrumentors_init();
    if(result)
    {
        pr_err("Failed to init instrumentors support.");
        return result;
    }
    
    return 0;
}

void test_cleanup(void)
{
    kedr_coi_instrumentors_destroy();
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
        kedr_coi_instrumentor_destroy(instrumentor, NULL);
        return result;
    }
    
    if(object.ops != &ops)
    {
        pr_err("Instrumentor change operations of the object, but shouldn't instrument object at all.");
        kedr_coi_instrumentor_forget(instrumentor, &object, 0);
        kedr_coi_instrumentor_destroy(instrumentor, NULL);
        return -EINVAL;
    }

    kedr_coi_instrumentor_forget(instrumentor, &object, 0);
    if(result < 0)
    {
        pr_err("Fail to forget object.");
        kedr_coi_instrumentor_destroy(instrumentor, NULL);
        return -1;
    }

    kedr_coi_instrumentor_destroy(instrumentor, NULL);
    return 0;
}
