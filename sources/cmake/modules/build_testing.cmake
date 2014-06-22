# Different functions for use when test build configuration.
#
# Key difficulty for this testing is that programs and other deliverables,
# may use paths to other project's files.
# Normally, when build these deliverables, them should be configured
# to use install paths.
# But for test these deliverables in build configuration, them
# should be reconfigured for use paths in build tree.

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
