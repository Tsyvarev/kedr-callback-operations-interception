########################################################################
# Test-related macros and functions
########################################################################
include(cmake_useful)
include(kbuild_system)

# Location of this CMake module
set(kedr_coi_testing_this_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules")

# Assuming that subdirectory means subcomponent and subtests,
# use common prefixes for names of subtests.
set(kedr_coi_test_prefix)

# Set prefix for all subsequent tests' names.
# Multiple prefixes are stacked.
# It is allowed to set multiple prefixes in one macro call.
macro (kedr_coi_test_add_prefix prefix)
    foreach(prefix_real ${prefix} ${ARGN})
        if(kedr_coi_test_prefix)
            set(kedr_coi_test_prefix "${kedr_coi_test_prefix}.${prefix_real}")
        else(kedr_coi_test_prefix)
            set(kedr_coi_test_prefix "${prefix_real}")
        endif(kedr_coi_test_prefix)
    endforeach(prefix_real ${prefix} ${ARGN})
endmacro (kedr_coi_test_add_prefix prefix)

## For internal use - get full test name
function(__kedr_coi_test_get_full_name RESULT_VAR test_name)
    string(REGEX MATCH "\\.[0123456789]+$" is_numbered ${test_name})
    if(NOT is_numbered)
        set(test_name "${test_name}.01")
    endif(NOT is_numbered)
    
    if(kedr_coi_test_prefix)
        set(${RESULT_VAR} "${kedr_coi_test_prefix}.${test_name}" PARENT_SCOPE)
    else(kedr_coi_test_prefix)
        set(${RESULT_VAR} "${test_name}" PARENT_SCOPE)
    endif(kedr_coi_test_prefix)
endfunction(__kedr_coi_test_get_full_name RESULT_VAR test_name)

# When we are building KEDR COI for another system (cross-build), testing is
# disabled. This is because the tests need the build tree.
# In the future, the tests could be prepared that need only the installed 
# components of KEDR COI. It could be a separate test suite.

# This macro enables testing support and performs other initialization tasks.
# It should be used in the top-level CMakeLists.txt file before 
# add_subdirectory () calls.
macro (kedr_coi_test_init)
    if (NOT CMAKE_CROSSCOMPILING)
	enable_testing ()
	if(KERNEL_PART_ONLY)
	    # From kernel part run also user part.
	    configure_file("${kedr_coi_testing_this_module_dir}/kedr_coi_testing_files/test_all_from_kernel.sh.in"
		"${CMAKE_BINARY_DIR}/test_all_from_kernel.sh" @ONLY)
	    add_custom_target (check 
		COMMAND /bin/sh "${CMAKE_BINARY_DIR}/test_all_from_kernel.sh"
	    )
	else(KERNEL_PART_ONLY)
	    add_custom_target (check 
	    COMMAND ${CMAKE_CTEST_COMMAND}
	    )
	endif(KERNEL_PART_ONLY)
	add_custom_target (build_tests)
	add_dependencies (check build_tests)
    endif (NOT CMAKE_CROSSCOMPILING)
    
    # In user part test shell scripts should use this cmake variable for refer
    # to build directory of kernel part.
    if(KERNEL_PART)
	set(test_shell_kernel_build_dir "${CMAKE_BUILD_DIRECTORY}")
    else(KERNEL_PART)
	set(test_shell_kernel_build_dir "\${kernel_part_build_dir}")
    endif(KERNEL_PART)
endmacro (kedr_coi_test_init)

# Use this macro to specify an additional target to be built before the tests
# are executed.
macro (kedr_coi_test_add_target target_name)
    if (NOT CMAKE_CROSSCOMPILING)
        set_target_properties (${target_name}
            PROPERTIES EXCLUDE_FROM_ALL true
        )
        add_dependencies (build_tests ${target_name})
    endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_coi_test_add_target target_name)


# This function adds a test script (a Bash script, actually) to the set of
# tests for the package. The script may reside in current source or binary 
# directory (the source directory is searched first).
function (kedr_coi_test_add_script test_name script_file)
    if (NOT CMAKE_CROSSCOMPILING)
        set (TEST_SCRIPT_FILE)
        to_abs_path (TEST_SCRIPT_FILE ${script_file})
        __kedr_coi_test_get_full_name(test_full_name "${test_name}")
            
        add_test (${test_full_name}
            /bin/bash ${TEST_SCRIPT_FILE} ${ARGN}
        )
    endif (NOT CMAKE_CROSSCOMPILING)
endfunction (kedr_coi_test_add_script)

# kedr_coi_test_add_kernel(test_name module_name sources ... [DEPENDS module ...])
#
# This function adds test as kernel space code.
# This code is wrapped into kernel module, which is built during
# 'build_tests' target and loaded into kernel during 'check'ing.
#
# 'test_name' is the name of the test
# 'module_name' is the name of the module created
#    (in the current binary directory)
# 'source' is source file(s) which contained test. This file(s) should
# export 3 functions:
# int test_init(void)
#  -initialize test, return 0 on successfull initialization and negative error code otherwise
# int test_run(void)
#  -execute test actions and return 0 on success and negative error code(usually -1) on fail
# void test_cleanup(void)
#  -clean test after running(note, that running may fail)
#
# Possible source files are the same as in kbuild_add_module() function
# and everything that affects on module created by kbuild_add_module()
# function is affects on the test module created by this function
# (e.g. kbuild_include_directory).
#
# If test code use definitions from other modules, these modules
# should be enumerated after DEPENDS keyword(NOT implemented).

function(kedr_coi_test_add_kernel test_name module_name source)
    if (NOT CMAKE_CROSSCOMPILING)
        set(test_kernel_code_source
            "${kedr_coi_testing_this_module_dir}/kedr_coi_testing_files/test_kernel_code.c"
        )
        set(test_kernel_code_script
            "${kedr_coi_testing_this_module_dir}/kedr_coi_testing_files/test_kernel_code.sh"
        )
        
        kbuild_add_module(${module_name} "test_kernel_code.c"
            ${source} ${ARGN}
        )
        rule_copy_file("test_kernel_code.c" "${test_kernel_code_source}")
        
           set_target_properties (${module_name}
            PROPERTIES EXCLUDE_FROM_ALL true
        )
        add_dependencies (build_tests ${module_name})

        __kedr_coi_test_get_full_name(test_full_name "${test_name}")
        
        add_test ("${test_full_name}" /bin/bash ${test_kernel_code_script}
            "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
            ${kedr_coi_test_kernel_dependencies}
        )
    endif (NOT CMAKE_CROSSCOMPILING)
endfunction(kedr_coi_test_add_kernel test_name module_name source)

# kedr_coi_test_add_kernel_dependency(module_name ...)
#
# If test's kernel code (kedr_coi_test_add_kernel())
# use symbols from other modules, this function should be called
# before adding the test. Parameters, given to function,
# will be interpreted as names of modules which should be loaded
# before test and unloaded after it finished.

function(kedr_coi_test_add_kernel_dependency module_name)
    list(APPEND kedr_coi_test_kernel_dependencies ${module_name} ${ARGN})
    set(kedr_coi_test_kernel_dependencies ${kedr_coi_test_kernel_dependencies} PARENT_SCOPE)
endfunction(kedr_coi_test_add_kernel_dependency module_name)

# Use this macro instead of add_subdirectory() for the subtrees related to 
# testing of the package.

# We could use other kedr_coi_*test* macros to disable the tests when 
# cross-building, but the rules of Kbuild system (concerning .symvers,
# etc.) still need to be disabled explicitly. So it is more reliable to 
# just turn off each add_subdirectory(tests) in this case.
macro (kedr_coi_test_add_subdirectory subdir)
    if (NOT CMAKE_CROSSCOMPILING)
        add_subdirectory(${subdir})
    endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_coi_test_add_subdirectory subdir)
########################################################################
