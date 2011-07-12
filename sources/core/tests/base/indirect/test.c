/*
 * Test base functionality of indirect interceptor.
 */

#include <kedr-coi/operations_interception.h>

int test_failed = 0;

struct test_object;
struct test_operations
{
    void* some_field;
    int (*do_something)(struct test_object* object, int value);
};

struct test_object
{
    char some_fields[13];
    const struct test_operations* ops;
};


struct kedr_coi_interceptor* interceptor;

typedef int (*do_something_t)(struct test_object* object, int value);
typedef void (*do_something_pre_t)(struct test_object* object, int value,
    struct kedr_coi_operation_call_info* call_info);
typedef void (*do_something_post_t)(struct test_object* object, int value,
    int returnValue,
    struct kedr_coi_operation_call_info* call_info);

static struct kedr_coi_intermediate_info do_something_intermediate_info;

static int repl_was_called;
//Model implementation of intermediate function
static int do_something_repl(struct test_object* object, int value)
{
    struct kedr_coi_operation_call_info call_info;
    do_something_t orig;
    int returnValue;
    
    repl_was_called = 1;
    call_info.return_address = __builtin_return_address(0);
    
    orig = (do_something_t)kedr_coi_interceptor_get_orig_operation(
        interceptor,
        object,
        offsetof(struct test_operations, do_something)
    );
            
    if(do_something_intermediate_info.pre != NULL)
    {
        do_something_pre_t* pre;
        for(pre = (do_something_pre_t*)do_something_intermediate_info.pre;
            *pre != NULL;
            pre++)
        {
            (*pre)(object, value, &call_info);
        }
    }
    
    returnValue = orig ? orig(object, value) : 0;

    if(do_something_intermediate_info.post != NULL)
    {
        do_something_post_t* post;
        for(post = (do_something_post_t*)do_something_intermediate_info.post;
            *post != NULL;
            post++)
        {
            (*post)(object, value, returnValue, &call_info);
        }
    }

    return returnValue;
}

static struct kedr_coi_intermediate intermediates[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something),
        .repl = do_something_repl,
        .info = &do_something_intermediate_info
    },
    {
        .operation_offset = -1
    }
};


static int orig_was_called;
int do_something_orig(struct test_object* object, int value)
{
    orig_was_called = 1;
    return 1;
}

static struct test_operations ops =
{
    .do_something = &do_something_orig
};


static int pre_was_called;
void do_something_handler_pre(int value,
    struct kedr_coi_operation_call_info* info)
{
    pre_was_called = 1;
    if(orig_was_called)
    {
        pr_err("Original operation was called before pre-handler.");
        test_failed = 1;
    }
}

static int post_was_called;
void do_something_handler_post(int value,
    int returnValue,
    struct kedr_coi_operation_call_info* info)
{
    post_was_called = 1;
    if(!orig_was_called)
    {
        pr_err("Original operation wasn't called before post-handler.");
        test_failed = 1;
    }
}

static struct kedr_coi_handler_pre handlers_pre[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something),
        .func = (void*)&do_something_handler_pre,
    },
    {
        .operation_offset = -1
    }

};

static struct kedr_coi_handler_post handlers_post[] =
{
    {
        .operation_offset = offsetof(struct test_operations, do_something),
        .func = (void*)&do_something_handler_post,
    },
    {
        .operation_offset = -1
    }
};


static struct kedr_coi_payload payload =
{
    .handlers_pre = handlers_pre,
    .handlers_post = handlers_post,
};

//********************Test infrastructure*****************************

int test_init(void)
{
    int result;
    interceptor = kedr_coi_interceptor_create("test_interceptor",
        offsetof(struct test_object, ops),
        sizeof(struct test_operations),
        intermediates);
    
    if(interceptor == NULL)
    {
        pr_err("Failed to create interceptor.");
        return -EINVAL;
    }
    
    result = kedr_coi_payload_register(interceptor, &payload);
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
        kedr_coi_payload_unregister(interceptor, &payload);
        kedr_coi_interceptor_destroy(interceptor);
        return result;
    }
    
    return 0;
}

void test_cleanup(void)
{
    kedr_coi_interceptor_stop(interceptor);
    kedr_coi_payload_unregister(interceptor, &payload);
    kedr_coi_interceptor_destroy(interceptor);
}

int test_run(void)
{
    int result;
    
    struct test_object object =
    {
        .ops = &ops
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
    
    orig_was_called = 0;
    pre_was_called = 0;
    post_was_called = 0;
    repl_was_called = 0;
    
    object.ops->do_something(&object, 5);
    
    if(!repl_was_called)
    {
        pr_err("For wathced object intermediate operation wasn't called.");
        return -EINVAL;
    }
    
    if(!pre_was_called)
    {
        pr_err("Pre handler wasn't called.");
        return -EINVAL;
    }
    if(!post_was_called)
    {
        pr_err("Post handler wasn't called.");
        return -EINVAL;
    }
    if(test_failed) return -EINVAL;
    
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

    orig_was_called = 0;
    pre_was_called = 0;
    post_was_called = 0;
    repl_was_called = 0;
    
    object.ops->do_something(&object, 5);

    if(repl_was_called)
    {
        pr_err("Intermediate operation was called after object was forgot.");
        return -EINVAL;
    }
    
    if(pre_was_called)
    {
        pr_err("Pre handler was called after object was forgot.");
        return -EINVAL;
    }

    if(post_was_called)
    {
        pr_err("Post handler was called after object was forgot.");
        return -EINVAL;
    }
    
    if(!orig_was_called)
    {
        pr_err("Original function wasn't called after object was forgot.");
        return -EINVAL;
    }

    return 0;
}

