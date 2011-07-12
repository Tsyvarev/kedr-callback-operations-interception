/*
 * Test that indirect instrumentor work with different objects correctly.
 */

#include "kedr_coi_instrumentor_internal.h"
#include "kedr_coi_global_map_internal.h"

#include "instrumentor_test_common.h"

#define OBJECTS_NUMBER 20
#define OPERATIONS_SET_NUMBER 3

// Return 1 if object included by objects' subset, 0 otherwise
static int objects_subset(int object_number)
{
    return (object_number + 1)  % 2;
}
/* Get set number according to object number */
static int object_number_to_set(int object_number)
{
    return object_number % OPERATIONS_SET_NUMBER;
}

/* Operations for test */
struct test_operations
{
    void* some_field;
    void (*do_something1)(int value);
    void (*unchanged_action1)(int value);
    void (*do_something2)(int value);
    void* other_fields[5];
    void (*unchanged_action2)(int value);
};
/* Object for test */
struct test_object
{
    int some_fields[10];
    const struct test_operations* ops;
};

/* Original operation1(several variants) */
static void do_something1_orig1(int value)
{
}

static void do_something1_orig2(int value)
{
}

static void do_something1_orig3(int value)
{
}

/* Replacement operation1 */
static void do_something1_repl(int value)
{
}


/* Original operation2(several variants) */
static void do_something2_orig1(int value)
{
}

static void do_something2_orig2(int value)
{
}

/* Replacement operation2 */
static void do_something2_repl(int value)
{
}



/* Unchanged operation1(several variants) */
static void unchanged_action1_1(int value)
{
}

static void unchanged_action1_2(int value)
{
}

static void unchanged_action1_3(int value)
{
}

/* Unchanged operation2(several variants) */
static void unchanged_action2_1(int value)
{
}

static void unchanged_action2_2(int value)
{
}

/* Several variants of operation sets.*/
static const struct test_operations ops1 =
{
    .do_something1 = &do_something1_orig1,
    .do_something2 = &do_something2_orig1,
    .unchanged_action1 = unchanged_action1_1,
    .unchanged_action2 = unchanged_action2_1,
};

static const struct test_operations ops2 =
{
    .do_something1 = &do_something1_orig2,
    .do_something2 = &do_something2_orig2,
    .unchanged_action1 = unchanged_action1_2,
    .unchanged_action2 = unchanged_action2_2,
};
static const struct test_operations ops3 =
{
    .do_something1 = &do_something1_orig3,
    .do_something2 = &do_something2_orig1,
    .unchanged_action1 = unchanged_action1_3,
    .unchanged_action2 = unchanged_action2_2,
};

static const struct test_operations* ops[] =
{
    &ops1,
    &ops2,
    &ops3,
};

struct kedr_coi_instrumentor_replacement replacements[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something1),
        .repl = (void*)&do_something1_repl
    },
    {
        .operation_offset = offsetof(struct test_operations, do_something2),
        .repl = (void*)&do_something2_repl
    },
    {
        .operation_offset = -1
    }
};


/* Replaced operations(several variants, correponds to ops & replacements)*/
static struct kedr_coi_test_replaced_operation replaced_operations1[] =
{
    {
        .offset = offsetof(struct test_operations, do_something1),
        .orig = (void*)&do_something1_orig1,
        .repl = (void*)&do_something1_repl
    },
    {
        .offset = offsetof(struct test_operations, do_something2),
        .orig = (void*)&do_something2_orig1,
        .repl = (void*)&do_something2_repl
    },
    {
        .offset = -1
    }
};

static struct kedr_coi_test_replaced_operation replaced_operations2[] =
{
    {
        .offset = offsetof(struct test_operations, do_something1),
        .orig = (void*)&do_something1_orig2,
        .repl = (void*)&do_something1_repl
    },
    {
        .offset = offsetof(struct test_operations, do_something2),
        .orig = (void*)&do_something2_orig2,
        .repl = (void*)&do_something2_repl
    },
    {
        .offset = -1
    }
};

static struct kedr_coi_test_replaced_operation replaced_operations3[] =
{
    {
        .offset = offsetof(struct test_operations, do_something1),
        .orig = (void*)&do_something1_orig3,
        .repl = (void*)&do_something1_repl
    },
    {
        .offset = offsetof(struct test_operations, do_something2),
        .orig = (void*)&do_something2_orig1,
        .repl = (void*)&do_something2_repl
    },
    {
        .offset = -1
    }
};

static struct kedr_coi_test_replaced_operation* replaced_operations[] =
{
    replaced_operations1,
    replaced_operations2,
    replaced_operations3
};

/* Unaffected operations(several variants, correponds to ops)*/
static struct kedr_coi_test_unaffected_operation unaffected_operations1[] =
{
    {
        .offset = offsetof(struct test_operations, unchanged_action1),
        .orig = (void*)&unchanged_action1_1,
    },
    {
        .offset = offsetof(struct test_operations, unchanged_action2),
        .orig = (void*)&unchanged_action2_1,
    },
    {
        .offset = -1
    }
};

static struct kedr_coi_test_unaffected_operation unaffected_operations2[] =
{
    {
        .offset = offsetof(struct test_operations, unchanged_action1),
        .orig = (void*)&unchanged_action1_2,
    },
    {
        .offset = offsetof(struct test_operations, unchanged_action2),
        .orig = (void*)&unchanged_action2_2,
    },
    {
        .offset = -1
    }
};

static struct kedr_coi_test_unaffected_operation unaffected_operations3[] =
{
    {
        .offset = offsetof(struct test_operations, unchanged_action1),
        .orig = (void*)&unchanged_action1_3,
    },
    {
        .offset = offsetof(struct test_operations, unchanged_action2),
        .orig = (void*)&unchanged_action2_2,
    },
    {
        .offset = -1
    }
};

static struct kedr_coi_test_unaffected_operation* unaffected_operations[] =
{
    unaffected_operations1,
    unaffected_operations2,
    unaffected_operations3
};

static struct test_object objects[OBJECTS_NUMBER];

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
    int i;
    
    struct kedr_coi_instrumentor* instrumentor;

    BUILD_BUG_ON(ARRAY_SIZE(ops) != OPERATIONS_SET_NUMBER);
    // Initialize objects operations
    for(i = 0; i < OBJECTS_NUMBER; i++)
    {
        objects[i].ops = ops[object_number_to_set(i)];
    }
    
    
    instrumentor = kedr_coi_instrumentor_create_indirect(
        offsetof(struct test_object, ops),
        sizeof(struct test_operations),
        replacements);
    
    if(instrumentor == NULL)
    {
        pr_err("Failed to create indirect instrumentor.");
        return -1;
    }
    
    // Instrument all objects
    for(i = 0; i < OBJECTS_NUMBER; i++)
    {
        result = kedr_coi_instrumentor_watch(instrumentor, &objects[i]);
        if(result < 0)
        {
            pr_err("Fail to watch for an object %d.", i);
            for(--i; i > 0; i--)
            {
                kedr_coi_instrumentor_forget(instrumentor, &objects[i], 0);
            }
            kedr_coi_instrumentor_destroy(instrumentor);
            return result;
        }
    }
    
    // Check that instrumentation was correct
    for(i = 0; i < OBJECTS_NUMBER; i++)
    {
        struct test_object* object = &objects[i];
        int set_number = object_number_to_set(i);
        
        result = check_object_instrumented(object,
            instrumentor,
            object->ops,
            replaced_operations[set_number],
            unaffected_operations[set_number]);

        if(result)
        {
            pr_err("Instrumentation of object %d is incorrect.", i);
            for(i = 0; i < OBJECTS_NUMBER; i++)
            {
                kedr_coi_instrumentor_forget(instrumentor, &objects[i], 0);
            }
            kedr_coi_instrumentor_destroy(instrumentor);
            return result;
        }
    }    

    // Deinstrument some objects
    for(i = 0; i < OBJECTS_NUMBER; i++)
    {
        if(!objects_subset(i)) continue;
        result = kedr_coi_instrumentor_forget(instrumentor, &objects[i], 0);
        if(result < 0)
        {
            pr_err("Fail to forget object %d.", i);
            /*
             * Do not uninstrument other objects. Memory should be freed
             * when instrumentor is destroyed.
             */
            kedr_coi_instrumentor_destroy(instrumentor);
            return result;
        }
    }
    // check objects again
    for(i = 0; i < OBJECTS_NUMBER; i++)
    {
        struct test_object* object = &objects[i];
        int set_number = object_number_to_set(i);
        
        result = objects_subset(i)
            ? check_object_uninstrumented(object,
                object->ops,
                replaced_operations[set_number],
                unaffected_operations[set_number])
            : check_object_instrumented(object,
                instrumentor,
                object->ops,
                replaced_operations[set_number],
                unaffected_operations[set_number]);

        if(result)
        {
            if(objects_subset(i))
                pr_err("Object %d was uninstrumented incorrectly.", i);
            else
                pr_err("Instrumentation of object %d became incorrect.", i);
            for(i = 0; i < OBJECTS_NUMBER; i++)
            {
                if(objects_subset(i)) continue;// already unistrumented
                kedr_coi_instrumentor_forget(instrumentor, &objects[i], 0);
            }
            kedr_coi_instrumentor_destroy(instrumentor);
            return result;
        }
    }    
    // Deinstrument rest objects
    for(i = 0; i < OBJECTS_NUMBER; i++)
    {
        if(objects_subset(i)) continue;//already uninstrumented
        result = kedr_coi_instrumentor_forget(instrumentor, &objects[i], 0);
        if(result < 0)
        {
            pr_err("Fail to forget object %d.", i);
            /*
             * Do not uninstrument other objects. Memory should be freed
             * when instrumentor is destroyed.
             */
            kedr_coi_instrumentor_destroy(instrumentor);
            return result;
        }
    }
    
    // Check that uninstrumentation was correct
    for(i = 0; i < OBJECTS_NUMBER; i++)
    {
        struct test_object* object = &objects[i];
        int set_number = object_number_to_set(i);
        
        if(objects_subset(i)) continue; //already checked
        
        result = check_object_uninstrumented(object,
            object->ops,
            replaced_operations[set_number],
            unaffected_operations[set_number]);

        if(result)
        {
            pr_err("Uninstrumentation of object %d is incorrect.", i);
            kedr_coi_instrumentor_destroy(instrumentor);
            return result;
        }
    }    

    kedr_coi_instrumentor_destroy(instrumentor);
    return 0;
}