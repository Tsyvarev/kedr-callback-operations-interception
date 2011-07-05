#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

#include "operations_include.h"

#ifndef OPERATIONS_STRUCT_TYPE
#error Macro 'OPERATIONS_STRUCT_TYPE' should be defined
#endif

#ifndef OPERATION_NAME
#error Macro 'OPERATION_NAME' should be defined
#endif

OPERATIONS_STRUCT_TYPE ops =
{
    .OPERATION_NAME = NULL,
};


static int __init
my_init(void)
{
#ifdef OPERATION_TYPE
	BUILD_BUG_ON(!__builtin_types_compatible_p(typeof(((OPERATIONS_STRUCT_TYPE *)0)->OPERATION_NAME), OPERATION_TYPE));
#endif /* OPERATION_TYPE */
    return 0;
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
