#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

#include <linux/fs.h>

struct dentry* dentry = NULL;
struct iattr* attr = NULL;

static int __init
my_init(void)
{
	//Module shouldn't be loaded!
	simple_setattr(dentry, attr);
    return 0;
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
