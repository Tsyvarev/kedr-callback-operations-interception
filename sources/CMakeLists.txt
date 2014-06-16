# Since 2.8.3 module CMakeParseArguments exists.
cmake_minimum_required(VERSION 2.8.3)

enable_language(C)
enable_language(CXX)
include(CMakeParseArguments)

#######################################################################
# Prohibit a common type of an in-source build.
# Note that building in a subdirectory in the source tree is still allowed 
# as it can be convenient.
string (COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" in_source)
if (in_source)
    message (FATAL_ERROR 
"It is not allowed to build the project in its top source directory."
)
endif () 

#######################################################################
# Names and versions
set(KEDR_COI_PACKAGE_NAME "kedr-coi")

set(KEDR_COI_VERSION_MAJOR 0)
set(KEDR_COI_VERSION_MINOR 1)
set(KEDR_COI_VERSION_MICRO 0)
set(KEDR_COI_VERSION_SUFFIX "" CACHE STRING
    "Version suffix, a string that should be appended ro the version"
)
set(KEDR_COI_VERSION 
"${KEDR_COI_VERSION_MAJOR}.${KEDR_COI_VERSION_MINOR}.${KEDR_COI_VERSION_MICRO}${KEDR_COI_VERSION_SUFFIX}"
)


#######################################################################
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

include(uninstall_target)

include(cmake_useful)

# Because Kbuild should be configured before multi_kernel,
# use user-defined parameter as guard for configure it.
if(NOT USER_PART_ONLY)
    find_package(Kbuild "2.6.32" REQUIRED)
endif(NOT USER_PART_ONLY)

include(multi_kernel)

# NB: 'path_prefixes' uses 'multi_kernel'
include(path_prefixes)
#######################################################################
# Kernel-specific path definitions.
if(KERNEL_PART)
    kernel_part_path(KEDR_COI_INSTALL_PREFIX_KMODULE
	"${KEDR_COI_KERNEL_INSTALL_PREFIX_KMODULE}")

    kernel_part_path(KEDR_COI_INSTALL_PREFIX_KSYMVERS
	"${KEDR_COI_KERNEL_INSTALL_PREFIX_KSYMVERS}")
    # In the build tree both kmodules and symvers are placed in their own dirs,
    # so there is no needs for have test variants for these paths.

    # Per-kernel includes
    kernel_part_path(KEDR_COI_INSTALL_INCLUDE_KERNEL_DIR
	"${KEDR_COI_KERNEL_INSTALL_INCLUDE_KERNEL_DIR}")
    kernel_part_path(KEDR_COI_TEST_INCLUDE_KERNEL_DIR
	"${KEDR_COI_KERNEL_TEST_INCLUDE_KERNEL_DIR}")
    add_testable_path (KEDR_COI_INCLUDE_KERNEL_DIR)
    
    kernel_part_path(KEDR_COI_INSTALL_PREFIX_INCLUDE_KERNEL
	"${KEDR_COI_KERNEL_INSTALL_PREFIX_INCLUDE_KERNEL}")
    kernel_part_path(KEDR_COI_TEST_PREFIX_INCLUDE_KERNEL
	"${KEDR_COI_KERNEL_TEST_PREFIX_INCLUDE_KERNEL}")
    add_testable_path (KEDR_COI_PREFIX_INCLUDE_KERNEL)
endif(KERNEL_PART)

kernel_make_path(KEDR_COI_INSTALL_MAKE_INCLUDE_KERNEL_DIR
    "${KEDR_COI_KERNEL_INSTALL_INCLUDE_KERNEL_DIR}"
)
kernel_make_path(KEDR_COI_TEST_MAKE_INCLUDE_KERNEL_DIR
    "${KEDR_COI_KERNEL_TEST_INCLUDE_KERNEL_DIR}"
)
kernel_build_test_make_path(KEDR_COI_TEST_MAKE_INCLUDE_KERNEL_DIR)
add_testable_path (KEDR_COI_MAKE_INCLUDE_KERNEL_DIR)

if(KERNEL_PART)
    include(kbuild_system)
    include(kmodule)
endif(KERNEL_PART)

include(template_generation)

include(kedr_coi_testing)
#######################################################################
#######################################################################
# Initialize test-related stuff
kedr_coi_test_init()
kedr_coi_test_add_prefix("kedr_coi")
#######################################################################
#kedr_coi_install_library(library_name)
function(kedr_coi_install_library library_name)
    install(TARGETS ${library_name} LIBRARY
            DESTINATION ${KEDR_COI_INSTALL_PREFIX_LIB})
endfunction(kedr_coi_install_library library_name)
#kedr_coi_install_headers(install_subdir header_file [..])
function(kedr_coi_install_headers install_subdir)
    install(FILES ${header_file} ${ARGN}
            DESTINATION ${KEDR_COI_INSTALL_PREFIX_INCLUDE}/${install_subdir})
endfunction(kedr_coi_install_headers install_subdir)
if(KERNEL_PART)
    #kedr_coi_install_kmodule(kmodule_name)
    function(kedr_coi_install_kmodule kmodule_name)
	kbuild_install(TARGETS ${kmodule_name} MODULE
	    DESTINATION "${KEDR_COI_INSTALL_PREFIX_KMODULE}"
	)
    endfunction(kedr_coi_install_kmodule kmodule_name)
    #kedr_coi_install_symvers(kmodule_name)
    function(kedr_coi_install_symvers kmodule_name)
	kbuild_install(TARGETS ${kmodule_name} SYMVERS
	    DESTINATION "${KEDR_COI_INSTALL_PREFIX_KSYMVERS}"
	)
    endfunction(kedr_coi_install_symvers kmodule_name)
endif(KERNEL_PART)
#######################################################################

#######################################################################
#${CMAKE_SOURCE_DIR}/include - root of the include tree (see conventions).
#include_directories("${CMAKE_SOURCE_DIR}/include")
#kbuild_include_directories("${CMAKE_SOURCE_DIR}/include")
include_directories("${CMAKE_BINARY_DIR}/include")
kbuild_include_directories("${CMAKE_BINARY_DIR}/include")

#For configuration file
kbuild_include_directories("${CMAKE_BINARY_DIR}")
#######################################################################
# Make "Release" the default build type
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo."
      FORCE)
endif ()
message (STATUS "Build type is \"${CMAKE_BUILD_TYPE}\"")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(KERNEL_PART)
	kbuild_add_definitions(
	    "-Wall -Wextra -Wno-unused-parameter -DKEDR_COI_DEBUG -g"
	)
    endif(KERNEL_PART)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(KERNEL_PART)
	kbuild_add_definitions(
	    "-Wall"
	)
    endif(KERNEL_PART)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(KERNEL_PART)
	kbuild_add_definitions(
	    "-Wall -g"
	)
    endif(KERNEL_PART)
else (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "Incorrect build type: ${CMAKE_BUILD_TYPE}. Should be one of 'Debug', 'Release' or 'RelWithDebInfo'")
endif()

#######################################################################
if(KERNEL_PART)
    # Determine way of hlist_for_each_* macros usage
    check_hlist_for_each_entry()
endif(KERNEL_PART)
#######################################################################
# Top directory with jinja2 templates for different purposes.
set(COI_TEMPLATES_DIR "${CMAKE_SOURCE_DIR}/templates/") 
#######################################################################

user_build_path(KEDR_COI_TEST_INCLUDE_DIR)
if(USER_PART)
    # Only common includes are processed here.
    add_subdirectory(include)
endif(USER_PART)

if(KERNEL_PART)
    add_subdirectory(core)
endif(KERNEL_PART)

# For tests only.
kbuild_get_module_location(KEDR_COI_TEST_SHELL_CORE_MODULE "kedr_coi")
kernel_build_test_shell_path(KEDR_COI_TEST_SHELL_CORE_MODULE)

kernel_make_path(KEDR_COI_INSTALL_MAKE_CORE_SYMBOLS
    "${KEDR_COI_KERNEL_INSTALL_PREFIX_KSYMVERS}/kedr_coi.symvers"
)
kbuild_get_symvers_location(KEDR_COI_TEST_MAKE_CORE_SYMBOLS "kedr_coi")
kernel_build_test_make_path(KEDR_COI_TEST_MAKE_CORE_SYMBOLS)
add_testable_path(KEDR_COI_MAKE_CORE_SYMBOLS)

# Tools may be used by anyone.
add_subdirectory(tools)

if (NOT CMAKE_CROSSCOMPILING)
    # jinja2-yaml will be installed, so it can be used, e.g. in examples.
    set(KEDR_COI_INSTALL_MAKE_JY_TOOL "${KEDR_COI_INSTALL_PREFIX_EXEC_AUX}/jinja2-yaml.py")
    # For tests use same ${JY_TOOL}, as for build package itself
    # (defined in the tools/CMakeLists.txt).
    # But need to explicitely join elements with ' ' instead of ';'.
    string(REPLACE ";" " " KEDR_COI_TEST_MAKE_JY_TOOL "${JY_TOOL}")
    add_testable_path(KEDR_COI_MAKE_JY_TOOL)
endif (NOT CMAKE_CROSSCOMPILING)

# Templates are common.
set(KEDR_COI_INSTALL_PREFIX_TEMPLATES
    "${KEDR_COI_INSTALL_PREFIX_READONLY}/templates")
set(KEDR_COI_TEST_PREFIX_TEMPLATES
    "${CMAKE_SOURCE_DIR}/templates")
if(USER_PART)
    add_subdirectory(templates)
endif(USER_PART)
add_testable_path("KEDR_COI_PREFIX_TEMPLATES")


# But generated interceptors are per-kernel.
set(KEDR_COI_KERNEL_INSTALL_PREFIX_INTERCEPTORS
    "${KEDR_COI_INSTALL_PREFIX_READONLY}/interceptors/%kernel%")

set(KEDR_COI_TEST_PREFIX_INTERCEPTORS
    "${CMAKE_BINARY_DIR}/interceptors/")

kernel_make_path(KEDR_COI_INSTALL_MAKE_PREFIX_INTERCEPTORS
    ${KEDR_COI_KERNEL_INSTALL_PREFIX_INTERCEPTORS})
set(KEDR_COI_TEST_MAKE_PREFIX_INTERCEPTORS ${KEDR_COI_TEST_PREFIX_INTERCEPTORS})
kernel_build_test_make_path(KEDR_COI_TEST_MAKE_PREFIX_INTERCEPTORS)
add_testable_path (KEDR_COI_MAKE_PREFIX_INTERCEPTORS)

if(KERNEL_PART)
    kernel_part_path(KEDR_COI_INSTALL_PREFIX_INTERCEPTORS
	"${KEDR_COI_KERNEL_INSTALL_PREFIX_INTERCEPTORS}")

    add_subdirectory(interceptors)
endif(KERNEL_PART)



# Common tests.
if(USER_PART)
    add_subdirectory(tests)
endif(USER_PART)


# Examples, kernel and user division is inside.
if (NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(examples)
endif (NOT CMAKE_CROSSCOMPILING)

if(USER_PART)
    add_subdirectory(doc)
endif(USER_PART)

if(KERNEL_PART)
    configure_file("${CMAKE_SOURCE_DIR}/config_kernel.h.in" "${CMAKE_BINARY_DIR}/config_kernel.h")
endif(KERNEL_PART)

if(KERNEL_PART)
    kbuild_finalize_linking()
endif(KERNEL_PART)
#######################################################################
# TODO: Message should reflect KERNEL_PART_ONLY/ USER_PART_ONLY.
message (STATUS "Configured ${KEDR_COI_PACKAGE_NAME} version ${KEDR_COI_VERSION}")

#######################################################################
