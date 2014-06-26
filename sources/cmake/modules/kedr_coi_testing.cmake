########################################################################
# Test-related macros and functions
########################################################################
include(cmake_useful)
include(install_testing)

# Only user part may add tests.
#
# Kernel part is allowed only to add components(kernel modules, etc.)
# needed for testing.
if(USER_PART)
    include(install_testing_ctest)
endif(USER_PART)

# Location of this CMake module
set(kedr_coi_testing_this_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules")

# This macro enables testing support and performs other initialization tasks.
macro (kedr_coi_test_init test_install_dir)
    itesting_init(${test_install_dir})
    if(USER_PART)
	ictest_enable_testing(${test_install_dir})

	# Install common script for run tests implemented as kernel modules.
	install(PROGRAMS "${kedr_coi_testing_this_module_dir}/kedr_coi_testing_files/test_kernel_code.sh"
	    DESTINATION "${test_install_dir}/scripts"
	)
	set(_test_kernel_code_script "${test_install_dir}/scripts/test_kernel_code.sh")
    endif(USER_PART)
endmacro (kedr_coi_test_init test_install_dir)

# This function adds a test script (a Bash script, actually) to the set of
# tests for the package.
#
# Script should already be installed.
#
# NOTE: Usable only for USER_PART(because it adds test).
function (kedr_coi_test_add_script test_name script_file)
    ictest_add_test(${test_name} /bin/bash ${script_file} ${ARGN})
endfunction (kedr_coi_test_add_script)


# kedr_coi_test_add_kmodule(name <sources> ...)
#
# Add kernel module, which implements some test.
#
# Test is performed when module is loaded, returing 0 on success
# and non-zero on fail.
# (So module unloading should be performed only when test is passed.)
#
# <sources> are kernel source files which contained test.
# These files(together) should export next 3 functions:
# 1) int test_init(void)
#  -initialize test, return 0 on successfull initialization and negative error code otherwise
# 2) int test_run(void)
#  -execute test actions and return 0 on success and negative error code(usually -1) on fail
# 3) void test_cleanup(void)
#  -clean test after running(note, that running may fail)
#
# Possible source files are the same as in kbuild_add_module() function
# and everything that affects on module created by kbuild_add_module()
# function is affects on the test module created by this function
# (e.g. kbuild_include_directory or kbuild_link_module).
#
# NOTE: Usable only for KERNEL_PART.
function(kedr_coi_test_add_kmodule name source)
    # Explicitely create private copy of common source file.
    rule_copy_file("test_kernel_code.c"
	"${kedr_coi_testing_this_module_dir}/kedr_coi_testing_files/test_kernel_code.c")

    kbuild_add_module(${module_name} "test_kernel_code.c"
	${source} ${ARGN}
    )
endfunction(kedr_coi_test_add_kmodule name source)

# kedr_coi_test_add_kernel(<test_name> <test_module> [<dep_modules> ...])
#
# Add test, which use kernel module <test_module> created with
# kedr_coi_test_add_kmodule().
# If test module use definitions from other modules, them should
# be listed after <test_module> in order of loading.
#
# Each of <test_module> and <dep_modules> should be one of:
#  - full path to the module
#  - full path to the module with %kernel% substring, which will be 
#      replaced with currently loaded kernel version at runtime.
#
# NOTE: Usable only for USER_PART(because it adds test).
function(kedr_coi_test_add_kernel test_name test_module)
    kedr_coi_test_add_script(${test_name} ${_test_kernel_code_script}
	${test_module}
	${ARGN}
    )
endfunction(kedr_coi_test_add_kernel test_name test_module)
########################################################################
