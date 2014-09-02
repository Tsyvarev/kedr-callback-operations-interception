/*
 * Check that kedr_coi_default_mechanism_selector returns appropriate
 * values for different memory areas.
 */
#include <kedr-coi/operations_interception.h>

#include <linux/sched.h> /* 'current' task struct definition. */

char buf_writable[10];

struct sample_ops_t
{
    void (*op1)(int);
};

static void op1_impl(int i)
{
    (void)i;
    return;
}

/* Const data should be initialized for being compiled into RO section.*/
static const struct sample_ops_t buf_possibly_ro =
{
    .op1 = op1_impl,
};



int test_init(void)
{
    return 0;
}

int test_run(void)
{
    bool res;
    
    // #1 - module private data
    res = kedr_coi_default_mechanism_selector(buf_writable);
    
    if(!res)
    {
        pr_info("kedr_coi_default_mechanism_selector should return true for module's writable addresses.");
        return -EINVAL;
    }
    
    // #2 - module private readonly data
    res = kedr_coi_default_mechanism_selector(&buf_possibly_ro);
    
// Currently returned value is depend from module's load implementation.
#ifdef CONFIG_DEBUG_SET_MODULE_RONX
    if(res)
    {
        pr_info("kedr_coi_default_mechanism_selector should return false for readonly module's data");
        return -EINVAL;
    }
#else
    if(!res)
    {
        pr_info("kedr_coi_default_mechanism_selector should return true for readonly module's data, if module loading implementation doesn't protect them");
        return -EINVAL;
    }
#endif /* CONFIG_DEBUG_SET_MODULE_RONX */

    if(res)
    {
        // Check that address is really writable.
        ((struct sample_ops_t*)&buf_possibly_ro)->op1 = NULL;
    }
    
    // #3 - kernel data
    res = kedr_coi_default_mechanism_selector(current);
    
    if(res)
    {
        pr_info("kedr_coi_default_mechanism_selector should return false for kernel's data");
        return -EINVAL;
    }
    
    return 0;
}

void test_cleanup(void)
{
    return;
}
