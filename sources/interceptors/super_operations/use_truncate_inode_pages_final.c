#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

#include <linux/fs.h>
// truncate_inode_pages_final is really defined there
#include <linux/mm.h>

static int __init
my_init(void)
{
    pr_info("func: %p", truncate_inode_pages_final);
    return 0;
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
