# Declare variables for path prefixes for different types of files.
# NB: depends on 'multi_kernel' and 'uninstall_target'.


# List of path variables, for use in configurable components
# (programs, scripts and so on) intended for install, but which may be
# tested in build environment.
#
# Each such path variable should start with 'KEDR_COI'.
set (KEDR_COI_TESTABLE_PATHS
    KEDR_COI_PREFIX_EXEC
    KEDR_COI_PREFIX_READONLY
    KEDR_COI_PREFIX_GLOBAL_CONF
    KEDR_COI_PREFIX_LIB
    KEDR_COI_INCLUDE_DIR
    KEDR_COI_PREFIX_INCLUDE 
    KEDR_COI_PREFIX_TEMP_SESSION
    KEDR_COI_PREFIX_TEMP
    KEDR_COI_PREFIX_STATE
    KEDR_COI_PREFIX_CACHE
    KEDR_COI_PREFIX_VAR
    KEDR_COI_PREFIX_DOC 
    KEDR_COI_PREFIX_EXAMPLES
)

# kedr_coi_load_install_paths()
#
# Load 'install' configuration for paths.
#
# For every variable KEDR_COI_<suffix>, listed in KEDR_COI_TESTABLE_PATHS,
# set its value to the value of variable KEDR_COI_INSTALL_<suffix>.
macro(kedr_coi_load_install_paths)
    foreach(var ${KEDR_COI_TESTABLE_PATHS})
        string(REGEX REPLACE "^KEDR_COI" "KEDR_COI_INSTALL" _install_var ${var})
	set(${var} ${${_install_var}})
    endforeach(var ${KEDR_COI_TESTABLE_PATHS})
endmacro(kedr_coi_load_install_paths)

# kedr_coi_load_test_paths()
#
# Load 'test' configuration for paths.
#
# For every variable KEDR_COI_<suffix>, listed in KEDR_COI_TESTABLE_PATHS,
# set its value to the value of variable KEDR_COI_TEST_<suffix>.

macro(kedr_coi_load_test_paths)
    foreach(var ${KEDR_COI_TESTABLE_PATHS})
        string(REGEX REPLACE "^KEDR_COI" "KEDR_COI_TEST" _test_var ${var})
	set(${var} ${${_test_var}})
    endforeach(var ${KEDR_COI_TESTABLE_PATHS})
endmacro(kedr_coi_load_test_paths)


# add_testable_path (path_variable ...)
# Add path variable(s) to the KEDR_COI_TESTABLE_PATHS list, so it will
# be filled  automatically when kedr_coi_load_[install|test]_paths() called.
#
# You should prepare correponded install and test variants for all added
# variables.
macro(add_testable_path path_variable)
    list(APPEND KEDR_COI_TESTABLE_PATHS ${path_variable} ${ARGN})
endmacro(add_testable_path path_variable)

########################################################################
# Kernel-dependent paths.
#
# Some deliverables may depends on linux kernel.
#
# For make "one user installation for several kernels" paradigm works,
# installation directory for that deliverables should include
# kernel-version part(like "3.10.2-generic").
#
# So, components installed by user installation can determine at runtime,
# which kernel-dependent deliverable should be used on currently loaded system.
# They do selection using 'uname -r' request.
#
# For define variables represented kernel-dependent directories,
# we use strings containing "%kernel%" stem.
#
# By conventions, kernel-dependent variables
# has 'KEDR_COI_KERNEL' prefix.
#
# NOTE: There is no reason to make such variables to be automatically
# filled using add_testable_path().

# kernel_path(kernel_version RESULT_VARIABLE pattern ...)
#
# Form concrete path representation from kernel-dependent pattern(s).
# Replace occurence of %kernel% in pattern(s) with given @kernel_version string.
# Result is stored in the RESULT_VARIABLE.
#
# @kernel_version may be concrete version of the kernel,
# or variable reference in some language.
macro(kernel_path kernel_version RESULT_VARIABLE pattern)
    string(REPLACE "%kernel%" "${kernel_version}" ${RESULT_VARIABLE} ${pattern} ${ARGN})
endmacro(kernel_path kernel_version RESULT_VARIABLE pattern)


########################################################################
# Install paths and prefixes.
#
# Follow conventions about paths listed in
#   devel-docs/general/path_conventions.txt
# in kedr-devel package.

# Determine type of installation
string(REGEX MATCH "(^/opt|^/usr|^/$)" IS_GLOBAL_INSTALL ${CMAKE_INSTALL_PREFIX})
if(IS_GLOBAL_INSTALL)
    set(KEDR_COI_INSTALL_TYPE "global")
    if(CMAKE_MATCH_1 STREQUAL "/opt")
        message("Global installation into /opt")
        set(KEDR_COI_INSTALL_GLOBAL_IS_OPT "opt")
    else(CMAKE_MATCH_1 STREQUAL "/opt")
    message("Global installation")
    endif(CMAKE_MATCH_1 STREQUAL "/opt")
else(IS_GLOBAL_INSTALL)
    message("Local installation")
    set(KEDR_COI_INSTALL_TYPE "local")
endif(IS_GLOBAL_INSTALL)

# 1
set(KEDR_COI_INSTALL_PREFIX_EXEC
        "${CMAKE_INSTALL_PREFIX}/bin")
set(KEDR_COI_INSTALL_PREFIX_EXEC_AUX
        "${CMAKE_INSTALL_PREFIX}/lib/${KEDR_COI_PACKAGE_NAME}")
# 2
set(KEDR_COI_INSTALL_PREFIX_READONLY
        "${CMAKE_INSTALL_PREFIX}/share/${KEDR_COI_PACKAGE_NAME}")
set(KEDR_COI_INSTALL_PREFIX_MANPAGE
        "${CMAKE_INSTALL_PREFIX}/share/man")
# 3
if(KEDR_COI_INSTALL_TYPE STREQUAL "global")
    set(KEDR_COI_INSTALL_PREFIX_GLOBAL_CONF
            "/etc/${KEDR_COI_PACKAGE_NAME}")
else(KEDR_COI_INSTALL_TYPE STREQUAL "global")
    set(KEDR_COI_INSTALL_PREFIX_GLOBAL_CONF
            "${CMAKE_INSTALL_PREFIX}/etc/${KEDR_COI_PACKAGE_NAME}")
endif(KEDR_COI_INSTALL_TYPE STREQUAL "global")
# 4
set(KEDR_COI_INSTALL_PREFIX_LIB
        "${CMAKE_INSTALL_PREFIX}/lib")
set(KEDR_COI_INSTALL_PREFIX_LIB_AUX
        "${CMAKE_INSTALL_PREFIX}/lib/${KEDR_COI_PACKAGE_NAME}")
# 5
set(KEDR_COI_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")

set(KEDR_COI_INSTALL_PREFIX_INCLUDE
        "${KEDR_COI_INSTALL_INCLUDE_DIR}/${KEDR_COI_PACKAGE_NAME}")
# 6
set(KEDR_COI_INSTALL_PREFIX_TEMP_SESSION
            "/tmp/${KEDR_COI_PACKAGE_NAME}")
# 7
if(KEDR_COI_INSTALL_TYPE STREQUAL "global")
    set(KEDR_COI_INSTALL_PREFIX_TEMP
                "/var/tmp/${KEDR_COI_PACKAGE_NAME}")
else(KEDR_COI_INSTALL_TYPE STREQUAL "global")
    set(KEDR_COI_INSTALL_PREFIX_TEMP
                "${CMAKE_INSTALL_PREFIX}/var/tmp/${KEDR_COI_PACKAGE_NAME}")
endif(KEDR_COI_INSTALL_TYPE STREQUAL "global")
# 8
if(KEDR_COI_INSTALL_TYPE STREQUAL "global")
    if(KEDR_COI_INSTALL_GLOBAL_IS_OPT)
        set(KEDR_COI_INSTALL_PREFIX_STATE
            "/var/opt/${KEDR_COI_PACKAGE_NAME}/lib/${KEDR_COI_PACKAGE_NAME}")
    else(KEDR_COI_INSTALL_GLOBAL_IS_OPT)
        set(KEDR_COI_INSTALL_PREFIX_STATE
            "/var/lib/${KEDR_COI_PACKAGE_NAME}")
    endif(KEDR_COI_INSTALL_GLOBAL_IS_OPT)
else(KEDR_COI_INSTALL_TYPE STREQUAL "global")
    set(KEDR_COI_INSTALL_PREFIX_STATE
        "${CMAKE_INSTALL_PREFIX}/var/lib/${KEDR_COI_PACKAGE_NAME}")
endif(KEDR_COI_INSTALL_TYPE STREQUAL "global")
# 9
if(KEDR_COI_INSTALL_TYPE STREQUAL "global")
    if(KEDR_COI_INSTALL_GLOBAL_IS_OPT)
        set(KEDR_COI_INSTALL_PREFIX_CACHE
            "/var/opt/${KEDR_COI_PACKAGE_NAME}/cache/${KEDR_COI_PACKAGE_NAME}")
    else(KEDR_COI_INSTALL_GLOBAL_IS_OPT)
        set(KEDR_COI_INSTALL_PREFIX_CACHE
            "/var/cache/${KEDR_COI_PACKAGE_NAME}")
    endif(KEDR_COI_INSTALL_GLOBAL_IS_OPT)
else(KEDR_COI_INSTALL_TYPE STREQUAL "global")
    set(KEDR_COI_INSTALL_PREFIX_CACHE
        "${CMAKE_INSTALL_PREFIX}/var/cache/${KEDR_COI_PACKAGE_NAME}")
endif(KEDR_COI_INSTALL_TYPE STREQUAL "global")
# 10
if(KEDR_COI_INSTALL_TYPE STREQUAL "global")
    if(KEDR_COI_INSTALL_GLOBAL_IS_OPT)
        set(KEDR_COI_INSTALL_PREFIX_VAR
            "/var/opt/${KEDR_COI_PACKAGE_NAME}")
    else(KEDR_COI_INSTALL_GLOBAL_IS_OPT)
        set(KEDR_COI_INSTALL_PREFIX_VAR
            "/var/opt/${KEDR_COI_PACKAGE_NAME}")
# Another variant
#        set(KEDR_COI_INSTALL_PREFIX_VAR
#            "/var/${KEDR_COI_PACKAGE_NAME}")
    endif(KEDR_COI_INSTALL_GLOBAL_IS_OPT)
else(KEDR_COI_INSTALL_TYPE STREQUAL "global")
    set(KEDR_COI_INSTALL_PREFIX_VAR
        "${CMAKE_INSTALL_PREFIX}/var/${KEDR_COI_PACKAGE_NAME}")
endif(KEDR_COI_INSTALL_TYPE STREQUAL "global")
# 11
set(KEDR_COI_INSTALL_PREFIX_DOC
    "${CMAKE_INSTALL_PREFIX}/share/doc/${KEDR_COI_PACKAGE_NAME}")

# Set derivative install path and prefixes
# additional, 1
set(KEDR_COI_KERNEL_INSTALL_PREFIX_KMODULE
    "${KEDR_COI_INSTALL_PREFIX_LIB}/modules/%kernel%/misc")
# Another variants:
#"${KEDR_COI_INSTALL_PREFIX_LIB}/modules/%kernel%/extra")
#"${KEDR_COI_INSTALL_PREFIX_LIB}/modules/%kernel%/updates")

# additional, 2
set(KEDR_COI_KERNEL_INSTALL_PREFIX_KSYMVERS
    "${KEDR_COI_INSTALL_PREFIX_LIB}/modules/%kernel%/symvers")
# additional, 3
set(KEDR_COI_INSTALL_PREFIX_KINCLUDE
    "${KEDR_COI_INSTALL_PREFIX_INCLUDE}")
# additional, 4
set(KEDR_COI_INSTALL_PREFIX_EXAMPLES
    "${KEDR_COI_INSTALL_PREFIX_READONLY}/examples")

# Kernel include files, which depends from kernel version.
set(KEDR_COI_KERNEL_INSTALL_INCLUDE_KERNEL_DIR
    "${CMAKE_INSTALL_PREFIX}/include-kernel/%kernel%")

set(KEDR_COI_KERNEL_INSTALL_PREFIX_INCLUDE_KERNEL
    "${KEDR_COI_KERNEL_INSTALL_INCLUDE_KERNEL_DIR}/${KEDR_COI_PACKAGE_NAME}-kernel")


# Default directory for configuration files
set(KEDR_COI_DEFAULT_CONFIG_DIR "${KEDR_COI_INSTALL_PREFIX_VAR}/configs")

########################################################################
# Path prefixes for tests

# Generally, test paths are install paths prefixed with 
# KEDR_COI_TEST_COMMON_PREFIX.
set(KEDR_COI_TEST_COMMON_PREFIX "/var/tmp/kedr-coi/test")

foreach(var ${KEDR_COI_PATHS})
    string(REGEX REPLACE "^KEDR_COI" "KEDR_COI_INSTALL" var_install ${var})
    string(REGEX REPLACE "^KEDR_COI" "KEDR_COI_TEST" var_test ${var})
    set(${var_test} "${KEDR_COI_TEST_COMMON_PREFIX}${var_install}")
endforeach(var ${KEDR_COI_PATHS})

# But some test paths are special. Rewrite them.

# Root of include tree in building package
#
# NOTE: this path is used only for tests, not for building package itself.
set(KEDR_COI_TEST_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include/install/")
set(KEDR_COI_TEST_PREFIX_INCLUDE "${KEDR_COI_TEST_INCLUDE_DIR}/${KEDR_COI_PACKAGE_NAME}")

set(KEDR_COI_KERNEL_TEST_INCLUDE_KERNEL_DIR
    "${CMAKE_BINARY_DIR}/include/install-kernel/%kernel%")

set(KEDR_COI_KERNEL_TEST_PREFIX_INCLUDE_KERNEL
    "${KEDR_COI_KERNEL_TEST_INCLUDE_KERNEL_DIR}/${KEDR_COI_PACKAGE_NAME}-kernel")

########################################################################
# [NB] All the "prefix" directories ending with ${KEDR_COI_PACKAGE_NAME}
# should be removed when uninstalling the package.
if(USER_PART)
    add_uninstall_dir(
	"${KEDR_COI_INSTALL_PREFIX_EXEC_AUX}"
	"${KEDR_COI_INSTALL_PREFIX_READONLY}"
	"${KEDR_COI_INSTALL_PREFIX_GLOBAL_CONF}"
	"${KEDR_COI_INSTALL_PREFIX_LIB_AUX}"
	"${KEDR_COI_INSTALL_PREFIX_INCLUDE}"
	"${KEDR_COI_INSTALL_PREFIX_TEMP_SESSION}"
	"${KEDR_COI_INSTALL_PREFIX_TEMP}"
	"${KEDR_COI_INSTALL_PREFIX_STATE}"
	"${KEDR_COI_INSTALL_PREFIX_CACHE}"
	"${KEDR_COI_INSTALL_PREFIX_VAR}"
	"${KEDR_COI_INSTALL_PREFIX_DOC}"
    )
endif(USER_PART)
if(KERNEL_PART)
    add_uninstall_dir(
	"${KEDR_COI_INSTALL_PREFIX_INCLUDE_KERNEL}"
    )
endif(KERNEL_PART)

# Test directory is build-dependent, so remove it for any configuration.
add_uninstall_dir(
    "${KEDR_COI_TEST_COMMON_PREFIX}"
)
########################################################################
