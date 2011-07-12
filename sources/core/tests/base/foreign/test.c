/*
 * Test base functionality of foreign interceptor.
 */

#include <kedr-coi/operations_interception.h>

int test_failed = 0;

struct test_object_foreign;
struct test_object;

struct test_operations_foreign
{
    void* some_field;
    int (*do_something)(struct test_object_foreign* object_foreign, int value);
};

struct test_object
{
    char some_fields[13];
    const struct test_operations_foreign* ops_foreign;
};

struct test_object_foreign
{
    char some_fileds[3];
    const struct test_operations_foreign* ops;
    struct test_object* object;
};


struct kedr_coi_interceptor* interceptor;

static struct kedr_coi_intermediate_foreign_info intermediate_info;

static int repl_was_called;
//Model implementation of intermediate function
static int do_something_repl(struct test_object_foreign* object_foreign, int value)
{
    struct test_object* object = object_foreign->object;
    repl_was_called = 1;
    
    if(kedr_coi_interceptor_foreign_restore_copy(interceptor,
        object,
        object_foreign))
    {
        pr_err("Failed to restore copy of foreign operations.");
        test_failed = 1;
    }

    if(intermediate_info.on_create_handlers)
    {
        kedr_coi_handler_foreign_t* on_create_handler;
        for(on_create_handler = intermediate_info.on_create_handlers;
            *on_create_handler != NULL;
            on_create_handler++)
        {
            (*on_create_handler)(object_foreign);
        }
    }

    return object_foreign->ops->do_something
        ? object_foreign->ops->do_something(object_foreign, value)
        : 0;
}

static struct kedr_coi_intermediate_foreign intermediates[] =
{
    {
        .operation_offset = offsetof(struct test_operations_foreign, do_something),
        .repl = do_something_repl,
    },
    {
        .operation_offset = -1
    }
};


static int orig_was_called;
int do_something_orig(struct test_object_foreign* object_foreign, int value)
{
    orig_was_called = 1;
    return 1;
}

static struct test_operations_foreign ops_foreign =
{
    .do_something = &do_something_orig
};


static int on_create_was_called;
void on_foreign_object_create(void* object_foreign)
{
    on_create_was_called = 1;
    if(orig_was_called)
    {
        pr_err("Original operation was called before on create handler.");
        test_failed = 1;
    }
}

static kedr_coi_handler_foreign_t on_create_handlers[] =
{
    &on_foreign_object_create,
    NULL
};


static struct kedr_coi_payload_foreign payload =
{
    .on_create_handlers = on_create_handlers
};

//********************Test infrastructure*****************************

int test_init(void)
{
    int result;
    interceptor = kedr_coi_interceptor_create_foreign("test_interceptor",
        offsetof(struct test_object, ops_foreign),
        sizeof(struct test_operations_foreign),
        offsetof(struct test_object_foreign, ops),
        intermediates,
        &intermediate_info);
    
    if(interceptor == NULL)
    {
        pr_err("Failed to create interceptor.");
        return -EINVAL;
    }
    
    result = kedr_coi_payload_foreign_register(interceptor, &payload);
    if(result)
    {
        pr_err("Failed to register payload.");
        kedr_coi_interceptor_destroy(interceptor);
        return result;
    }
    
    result = kedr_coi_interceptor_start(interceptor);
    if(result)
    {
        pr_err("Failed to start interceptor");
        kedr_coi_payload_foreign_unregister(interceptor, &payload);
        kedr_coi_interceptor_destroy(interceptor);
        return result;
    }
    
    return 0;
}

void test_cleanup(void)
{
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_foreign_unregister(interceptor, &payload);
    kedr_coi_interceptor_destroy(interceptor);
}

int test_run(void)
{
    int result;
    
    struct test_object object =
    {
        .ops_foreign = &ops_foreign
    };
    
    result = kedr_coi_interceptor_watch(interceptor, &object);
    
    if(result < 0)
    {
        pr_err("Failed to watch for an object.");
        return result;
    }
    
    if(result > 0)
    {
        pr_err("kedr_coi_interceptor_watch() returns positive value for "
            "first watching for an object.");
        return -EINVAL;
    }
    
    {
        //Create foreign object and copy operations into it.
        struct test_object_foreign object_foreign;
        object_foreign.ops = object.ops_foreign;
        object_foreign.object = &object;
        
        orig_was_called = 0;
        on_create_was_called = 0;
        repl_was_called = 0;
        
        object_foreign.ops->do_something(&object_foreign, 5);

        if(!repl_was_called)
        {
            pr_err("For foreign object intermediate operation wasn't called.");
            return -EINVAL;
        }
        
        if(!on_create_was_called)
        {
            pr_err("On create handler wasn't called.");
            return -EINVAL;
        }
        if(!orig_was_called)
        {
            pr_err("Original operation wasn't called.");
            return -EINVAL;
        }
        if(test_failed) return -EINVAL;
    }
    
    
    result = kedr_coi_interceptor_forget(interceptor, &object);
    if(result < 0)
    {
        pr_err("Failed to forget object.");
        return result;
    }
    
    if(result > 0)
    {
        pr_err("kedr_coi_instrumentor_forget() return positive value "
            "for object which was watched for.");
        return -EINVAL;
    }


    {
        //Create foreign object and copy operations into it.
        struct test_object_foreign object_foreign;
        object_foreign.ops = object.ops_foreign;
        object_foreign.object = &object;
        
        orig_was_called = 0;
        on_create_was_called = 0;
        repl_was_called = 0;
        
        object_foreign.ops->do_something(&object_foreign, 5);

        if(repl_was_called)
        {
            pr_err("For foreign object intermediate operation was called "
                "even after original object was forgot.");
            return -EINVAL;
        }
        
        if(!orig_was_called)
        {
            pr_err("Original operation wasn't called after original object "
                "was forgot.");
            return -EINVAL;
        }
        if(test_failed) return -EINVAL;
    }

    return 0;
}

