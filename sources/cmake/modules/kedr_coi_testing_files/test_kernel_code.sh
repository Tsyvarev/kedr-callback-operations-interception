#!/bin/sh

if test $# -ne 1; then
    printf "Usage: $0 <test_module>"
fi

test_module=$1

if ! insmod ${test_module}; then
    printf "Errors occures during the test, see system log.\n"
    exit 1
fi

if ! rmmod ${test_module}; then
    printf "Errors occures during test module unloading.\n"
    exit 1
fi