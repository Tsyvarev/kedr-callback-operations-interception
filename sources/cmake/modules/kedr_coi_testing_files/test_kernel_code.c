/*
 * Module wrapper for the kernel space test.
 */

#include <linux/module.h>

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

extern int test_init(void);
extern int test_run(void);
extern void test_cleanup(void);

static int __init
test_module_init(void)
{
    int result;
    
    result = test_init();
    if(result < 0)
    {
        pr_err("Failed to init test.");
        return result;
    }
    else if(result > 0)
    {
        pr_err("Test initialization function returns positive result.");
        return -EINVAL;
    }

    result = test_run();
    
    test_cleanup();

    if(result < 0)
    {
        pr_err("Test failed.");
        return result;
    }
    else if(result > 0)
    {
        pr_err("Test execution returns positive result.");
        return -EINVAL;
    }

    return 0;
}

static void __exit
test_module_exit(void)
{
}

module_init(test_module_init);
module_exit(test_module_exit);