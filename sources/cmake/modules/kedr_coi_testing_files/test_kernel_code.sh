#!/bin/sh

if test $# -lt 1; then
    printf "Usage: $0 <test_module> [dependency_module] ..."
fi

KERNEL_VERSION=`uname -r`

# get_precise_path <path>
#
# Replace possible %kernel% in path with current kernel version.
get_precise_path()
{
    echo "$1" | sed -e "s/%kernel%/${KERNEL_VERSION}/"
}
# get_module_name <path>
#
# Extract module name from <path>
get_module_name()
{
    # TODO: Archived modules may have exception aside from .ko. Process that case also.
    echo "$1" | sed -e "s/.\{0,\}\///; s/\.ko$//"
}

test_module=`get_precise_path $1`
shift

modules_for_unload=
while test $# -ne 0; do
    dependency_module=`get_precise_path $1`
    if ! /sbin/insmod $dependency_module; then
        printf "Failed to load module '$dependency_module' needed for test."
        for module in $modules_for_unload; do
            /sbin/rmmod $module
        done
        exit 1
    fi
    #reverse order of modules for unload
    module_name=`get_module_name $dependency_module`
    modules_for_unload="$module_name $modules_for_unload"
    shift
done

if ! /sbin/insmod ${test_module} ; then
    printf "Errors occures during the test, see system log.\n"
    for module in $modules_for_unload; do
        /sbin/rmmod $module
    done
    exit 1
fi

test_module_for_unload=`get_module_name $test_module`

if ! /sbin/rmmod ${test_module_for_unload}; then
    printf "Errors occures during test module unloading.\n"
    exit 1
fi

for module in $modules_for_unload; do
    if ! /sbin/rmmod $module; then
        printf "Errors occures while unloading dependency modules.\n"
        exit 1
    fi
done
