# Support for paradigm "one user part for many kernels".
#
# NB: Before use macros defined there, 'Kbuild' should be configured.
#
# All project's deliverables are divided into two groups.
# Deliverable in the first group, defined under "if(USER_PART)",
# is common for all kernels.
# Deliverable in the second group, defined under "if(KERNEL_PART)",
# is built and installed per-kernel.
#
# By default, both groups are compiled together, and resulted single
# installation work with only one kernel (for which it has been configured).
#
# For work with many kernels, user part should be configured with
#    -DUSER_PART_ONLY=ON
# cmake option.
# Kernel part should be configured with
#   -DKERNEL_PART_ONLY=ON
# cmake option into different
# build directories for every kernel. Also,
#    -DUSER_PART_BINARY_DIR=<user-build-dir>
# should be passed to every per-kernel configuration process.
#
# Note, that common definitions should be passed both to user part
# and kernel ones.
#
# Usually, per-kernel components are installed to directory,
# contained kernel version. So common components may chose per-kernel
# ones using version of currently run kernel.
#
# Thing become more complex if we want to refer to per-kernel build tree
# from common tools, or, vice-versa, to common build tree from per-kernel
# tools. This is usual when we prepare tests which works with build tree.
#
# Second reference(per-kernel tools -> common build tree) is simple:
# It should use USER_PART_BINARY_DIR cmake variable instead
# of CMAKE_BINARY_DIR.
#
# Reversed reference(common tools -> per-kernel build tree) is more tricky.
# Currently it supported only for common tests, which should be run from
# per-kernel testing process with
#   kernel_part_binary_dir
# environment variable set to ${CMAKE_BINARY_DIR}.
# 

# Environment variable used for refer to kernel version in deliverables.
# By default, it is "KERNEL", but it may be redefined by the project.
if(NOT multi_kernel_KERNEL_VAR)
    set(multi_kernel_KERNEL_VAR "KERNEL")
endif(NOT multi_kernel_KERNEL_VAR)

# Environment variable used for refer to kernel part build directory in common tests.
# By default, it is "KERNEL", but it may be redefined by the project.
if(NOT multi_kernel_KERNEL_BINARY_DIR_VAR)
    set(multi_kernel_KERNEL_BINARY_DIR_VAR "kernel_part_binary_dir")
endif(NOT multi_kernel_KERNEL_BINARY_DIR_VAR)


option(USER_PART_ONLY "Build only user-space part of the project" OFF)
option(KERNEL_PART_ONLY "Build only kernel-space part of the project" OFF)

if(USER_PART_ONLY AND KERNEL_PART_ONLY)
    message(FATAL_ERROR "At most one of KERNEL_PART_ONLY and USER_PART_ONLY can be specified")
endif(USER_PART_ONLY AND KERNEL_PART_ONLY)

# Setup macros defining each part.
if(NOT USER_PART_ONLY)
    set(KERNEL_PART ON)
endif(NOT USER_PART_ONLY)
if(NOT KERNEL_PART_ONLY)
    set(USER_PART ON)
endif(NOT KERNEL_PART_ONLY)


if(KERNEL_PART_ONLY)
    set(USER_PART_BINARY_DIR "" CACHE PATH "Build directory for corresponded user part.")
    if(NOT USER_PART_BINARY_DIR)
    message(FATAL_ERROR "USER_PART_BINARY_DIR should be specified when build only kernel part." )
    endif(NOT USER_PART_BINARY_DIR)
    
else(KERNEL_PART_ONLY)
    set(USER_PART_BINARY_DIR "${CMAKE_BINARY_DIR}")
endif(KERNEL_PART_ONLY)

# Helper. Replace given substring only if it is a prefix.
function(_replace_prefix match_prefix replace_prefix output_variable input)
    # Standard string(REPLACE) replace all substrings.
    # It is possible to match prefix only with string(REGEX REPLACE),
    # but it is required to escape all special REGEX sequences in match_prefix.
    # 
    # Find and replace prefix "by hand".
    
    # If prefix will not be found, output is equal to input.
    set(output_tmp "${input}")
    
    string(LENGTH ${match_prefix} _match_prefix_len)
    string(LENGTH ${input} _input_len)
    if(NOT _input_len LESS ${_match_prefix_len})
        string(SUBSTRING "${input}" 0 "${_match_prefix_len}" _input_prefix)
        if(_input_prefix STREQUAL ${match_prefix})
            math(EXPR _input_suffix_len "${_input_len}-${_match_prefix_len}")
            string(SUBSTRING "${input}" "${_match_prefix_len}" "${_input_suffix_len}" _input_suffix)
            set(output_tmp "${replace_prefix}${_input_suffix}")
        endif(_input_prefix STREQUAL ${match_prefix})
    endif(NOT _input_len LESS ${_match_prefix_len})
    
    set(${output_variable} "${output_tmp}" PARENT_SCOPE)
endfunction(_replace_prefix match_prefix replace_prefix output_variable input)

# Reinterpret PATH_VAR as referred to user part build dir.
#
# So, if path is prefixed with ${CMAKE_BINARY_DIR}, it will be prefixed
# with ${USER_PART_BINARY_DIR}. Result is stored into same PATH_VAR.
macro(user_build_path PATH_VAR)
    if(NOT USER_PART)
        _replace_prefix(${CMAKE_BINARY_DIR} ${USER_PART_BINARY_DIR} ${PATH_VAR} "${${PATH_VAR}}")
    endif(NOT USER_PART)
endmacro(user_build_path PATH_VAR)


# Reinterpret PATH_VAR as referred to kernel part build dir.
# Result may be used in the shell scripts for tests.
#
# So, if path is prefixed with ${CMAKE_BINARY_DIR}, it will be prefixed
# with value of '${multi_kernel_KERNEL_BINARY_DIR_VAR}' environment variable.
macro(kernel_build_test_shell_path PATH_VAR)
    if(NOT KERNEL_PART)
        _replace_prefix(${CMAKE_BINARY_DIR} "\${${multi_kernel_KERNEL_BINARY_DIR_VAR}}" ${PATH_VAR} "${${PATH_VAR}}")
    endif(NOT KERNEL_PART)
endmacro(kernel_build_test_shell_path RESULT_VAR path)

# Reinterpret PATH_VAR as referred to kernel part build dir.
# Result may be used in the make scripts for tests.
#
# So, if path is prefixed with ${CMAKE_BINARY_DIR}, it will be prefixed
# with value of '${multi_kernel_KERNEL_BINARY_DIR_VAR}' environment variable.
macro(kernel_build_test_make_path PATH_VAR)
    if(NOT KERNEL_PART)
        _replace_prefix(${CMAKE_BINARY_DIR} "\$(${multi_kernel_KERNEL_BINARY_DIR_VAR})" ${PATH_VAR} "${${PATH_VAR}}")
    endif(NOT KERNEL_PART)
endmacro(kernel_build_test_make_path RESULT_VAR path)


# Definition of ${multi_kernel_KERNEL_VAR} variable in shell and make scripts.
if(KERNEL_PART)
    set(multi_kernel_KERNEL_VAR_SHELL_DEFINITION "${multi_kernel_KERNEL_VAR}=${Kbuild_VERSION_STRING}")
    set(multi_kernel_KERNEL_VAR_MAKE_DEFINITION "${multi_kernel_KERNEL_VAR} := ${Kbuild_VERSION_STRING}")
else(KERNEL_PART)
    set(multi_kernel_KERNEL_VAR_SHELL_DEFINITION "${multi_kernel_KERNEL_VAR}=`uname -r`")
    set(multi_kernel_KERNEL_VAR_MAKE_DEFINITION "${multi_kernel_KERNEL_VAR} := \$(shell uname -r)")
endif(KERNEL_PART)

# Form kernel-dependent path from pattern.
# Result is usable in shell scripts.
#
# Replace %kernel% substring with '${multi_kernel_KERNEL_VAR}'
# variable reference.
macro(kernel_shell_path RESULT_VAR pattern)
    kernel_path("\${${multi_kernel_KERNEL_VAR}}" ${RESULT_VAR} ${pattern})
endmacro(kernel_shell_path RESULT_VAR pattern)

# Form kernel-dependent path from pattern.
# Result is usable in make scripts.
#
# Replace %kernel% substring with '${multi_kernel_KERNEL_VAR}'
# variable reference.
macro(kernel_make_path RESULT_VAR pattern)
    kernel_path("\$(${multi_kernel_KERNEL_VAR})" ${RESULT_VAR} ${pattern})
endmacro(kernel_make_path RESULT_VAR pattern)

# Form kernel-dependent path from pattern.
# Result is constant string.
# May be used only in KERNEL_PART.
#
# Replace %kernel% substring with value of '${Kbuild_VERSION_STRING}'
# variable.
macro(kernel_part_path RESULT_VAR pattern)
    if(NOT KERNEL_PART)
        message(FATAL_ERROR "kernel_part_path macro is usable only in KERNEL_PART.")
    endif(NOT KERNEL_PART)
    kernel_path("${Kbuild_VERSION_STRING}" ${RESULT_VAR} ${pattern})
endmacro(kernel_part_path RESULT_VAR pattern)
