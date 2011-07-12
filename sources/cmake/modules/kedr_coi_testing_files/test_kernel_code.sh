#!/bin/sh

if test $# -lt 1; then
    printf "Usage: $0 <test_module> [dependency_module] ..."
fi

test_module=$1
shift


dependency_modules=
while test $# -ne 0; do
    dependency_module=$1
    if ! /sbin/insmod $dependency_module; then
        printf "Failed to load module '$dependency_module' needed for test."
        for module in $dependency_modules; do
            /sbin/rmmod $module
        done
        exit 1
    fi
    #reverse order of modules for unload
    dependency_modules="$dependency_module $dependency_modules"
    shift
done


if ! /sbin/insmod ${test_module} ; then
    printf "Errors occures during the test, see system log.\n"
    for module in $dependency_modules; do
        /sbin/rmmod $module
    done
    exit 1
fi

if ! /sbin/rmmod ${test_module}; then
    printf "Errors occures during test module unloading.\n"
    exit 1
fi

for module in $dependency_modules; do
    if ! /sbin/rmmod $module; then
        printf "Errors occures while unloading dependency modules.\n"
        exit 1
    fi
done
