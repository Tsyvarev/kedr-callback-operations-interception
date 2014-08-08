# Since 2.8.3 module CMakeParseArguments exists.
cmake_minimum_required(VERSION 2.8.3)

enable_language(C)

project(kedr-coi)

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

include(CMakeParseArguments)
include(cmake_useful)

include(uninstall_target)

include(path_prefixes)
include(build_testing)

# Because Kbuild should be configured before multi_kernel,
# use user-defined parameter as guard for configure it.
if(NOT USER_PART_ONLY)
    find_package(Kbuild "2.6.32" REQUIRED)
endif(NOT USER_PART_ONLY)

include(multi_kernel)

fill_install_prefixes("${KEDR_COI_PACKAGE_NAME}" KEDR_COI KERNEL)
# Manually replace some prefixes for make all global installations consistent with DKMS.
# In the future, these paths may be changed in path_prefixes.cmake module.
if(KEDR_COI_INSTALL_TYPE MATCHES "GLOBAL")
    set(KEDR_COI_KERNEL_INSTALL_PREFIX_KMODULE "/lib/modules/%kernel%/extra")
    set(KEDR_COI_KERNEL_INSTALL_PREFIX_KSYMVERS "/lib/modules/%kernel%/symvers")
endif(KEDR_COI_INSTALL_TYPE MATCHES "GLOBAL")
#######################################################################
# Kernel-specific path definitions.
if(KERNEL_PART)
    kernel_part_path(KEDR_COI_INSTALL_PREFIX_KMODULE
	"${KEDR_COI_KERNEL_INSTALL_PREFIX_KMODULE}")

    kernel_part_path(KEDR_COI_INSTALL_PREFIX_KSYMVERS
	"${KEDR_COI_KERNEL_INSTALL_PREFIX_KSYMVERS}")

    # Per-kernel includes
    kernel_part_path(KEDR_COI_INSTALL_INCLUDE_KERNEL_DIR
	"${KEDR_COI_KERNEL_INSTALL_INCLUDE_KERNEL_DIR}")

    kernel_part_path(KEDR_COI_INSTALL_PREFIX_INCLUDE_KERNEL
	"${KEDR_COI_KERNEL_INSTALL_PREFIX_INCLUDE_KERNEL}")
endif(KERNEL_PART)

kernel_make_path(KEDR_COI_INSTALL_MAKE_INCLUDE_KERNEL_DIR
    "${KEDR_COI_KERNEL_INSTALL_INCLUDE_KERNEL_DIR}"
)


#######################################################################
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
#######################################################################
set(KEDR_INSTALL_DIR "" CACHE PATH "Directory where KEDR is installed(optional)")
if(KEDR_INSTALL_DIR)
    # Also fill install prefixes for KEDR package.
    fill_install_prefixes("kedr" "KEDR" BASE_INSTALL_PREFIX "${KEDR_INSTALL_DIR}" KERNEL)
    
    kernel_make_path(KEDR_INSTALL_MAKE_PREFIX_KSYMVERS
	"${KEDR_KERNEL_INSTALL_PREFIX_KSYMVERS}")
endif(KEDR_INSTALL_DIR)
#######################################################################
# Actually, 'DKMS' option has different meaning for USER_PART_ONLY and
# KERNEL_PART_ONLY builds.
option(DKMS "Integrate with DKMS" OFF)
if(DKMS)
    if(NOT KEDR_COI_INSTALL_TYPE MATCHES "GLOBAL")
	message(FATAL_ERROR "'DKMS' options is allowed only with global installation.")
    endif(NOT KEDR_COI_INSTALL_TYPE MATCHES "GLOBAL")

    if(USER_PART_ONLY)
	# For USER_PART_ONLY 'DKMS=on' means that need to additionally
	# install source files for future 'dkms add' call.
	include(dkms_system)
	
	dkms_add(dkms_common ${KEDR_COI_PACKAGE_NAME} ${KEDR_COI_VERSION})
	set_property(TARGET dkms_common PROPERTY DKMS_CMAKE_FLAGS
	    "-DDKMS=on"
	    "-DKERNEL_PART_ONLY=on"
	    "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
	)
    elseif(KERNEL_PART_ONLY)
	# For KERNEL_PART_ONLY 'DKMS=on' means that project is built by
	# DKMS, and core modules shouldn't be installed with cmake.
    else()
	message(FATAL_ERROR "DKMS option is allowed only for USER_PART_ONLY or KERNEL_PART_ONLY builds")
    endif(USER_PART_ONLY)
endif(DKMS)
#######################################################################
if(KERNEL_PART)
    # Redefine some default kbuild compiler flags before including 'kbuild_system'.
    set(KBUILD_C_FLAGS
	"-Wall"
	CACHE STRING "Compiler flags used by Kbuild system during all builds."
    )
    
    set(KBUILD_C_FLAGS_DEBUG
	"-Wextra -Wno-unused-parameter -DKEDR_COI_DEBUG -g"
	CACHE STRING "Compiler flags used by Kbuild system during debug builds."
    )

    include(kbuild_system)
    include(lookup_module_symbol)
    include(kmodule)
endif(KERNEL_PART)

include(template_generation)
#######################################################################
#kedr_coi_install_headers(install_subdir header_file [..])
function(kedr_coi_install_headers install_subdir)
    install(FILES ${header_file} ${ARGN}
	DESTINATION ${KEDR_COI_INSTALL_PREFIX_INCLUDE}/${install_subdir}
	COMPONENT "devel"
    )
endfunction(kedr_coi_install_headers install_subdir)
if(KERNEL_PART)
    #kedr_coi_install_kmodule(kmodule_name)
    function(kedr_coi_install_kmodule kmodule_name)
	kbuild_install(TARGETS ${kmodule_name} MODULE
	    DESTINATION "${KEDR_COI_INSTALL_PREFIX_KMODULE}"
	    COMPONENT "core-kernel"
	)
    endfunction(kedr_coi_install_kmodule kmodule_name)
    #kedr_coi_install_symvers(kmodule_name)
    function(kedr_coi_install_symvers kmodule_name)
	kbuild_install(TARGETS ${kmodule_name} SYMVERS
	    DESTINATION "${KEDR_COI_INSTALL_PREFIX_KSYMVERS}"
	    COMPONENT "devel-kernel"
	)
    endfunction(kedr_coi_install_symvers kmodule_name)
elseif(DKMS)
    # Common destination dir for DKMS modules.
    set(dkms_module_install_dir "extra")
endif(KERNEL_PART)
#######################################################################

#######################################################################
#${CMAKE_SOURCE_DIR}/include - root of the include tree (see conventions).
if(KERNEL_PART)
    kbuild_include_directories("${CMAKE_SOURCE_DIR}/include")

    #For configuration file
    kbuild_include_directories("${CMAKE_BINARY_DIR}")
endif(KERNEL_PART)
#######################################################################
# Make "Release" the default build type
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif ()
message (STATUS "Build type is \"${CMAKE_BUILD_TYPE}\"")
#######################################################################
if(KERNEL_PART)
    # Check whether Kbuild really can build modules.
    check_module_build()
    # Determine way of hlist_for_each_* macros usage
    check_hlist_for_each_entry()
endif(KERNEL_PART)
#######################################################################
# Top directory with jinja2 templates for different purposes.
set(COI_TEMPLATES_DIR "${CMAKE_SOURCE_DIR}/templates/") 
#######################################################################

if(USER_PART)
    # Only common includes are processed here.
    add_subdirectory(include)
endif(USER_PART)

if(KERNEL_PART)
    add_subdirectory(core)
elseif(DKMS)
    dkms_add_module(dkms_common "kedr_coi" "core/" "${dkms_module_install_dir}")
endif(KERNEL_PART)

kernel_make_path(KEDR_COI_INSTALL_MAKE_CORE_SYMBOLS
    "${KEDR_COI_KERNEL_INSTALL_PREFIX_KSYMVERS}/kedr_coi.symvers")

kernel_shell_path(KEDR_COI_INSTALL_SHELL_CORE_MODULE
    "${KEDR_COI_KERNEL_INSTALL_PREFIX_KMODULE}/kedr_coi.ko")


# Tools may be used by anyone.
add_subdirectory(tools)

if (NOT CMAKE_CROSSCOMPILING)
    # jinja2-yaml will be installed, so it can be used, e.g. in examples.
    set(KEDR_COI_INSTALL_MAKE_JY_TOOL "${KEDR_COI_INSTALL_PREFIX_EXEC_AUX}/jinja2-yaml.py")
endif (NOT CMAKE_CROSSCOMPILING)

# Templates are common.
set(KEDR_COI_TEMPLATES_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/templates")

set(KEDR_COI_INSTALL_PREFIX_TEMPLATES
    "${KEDR_COI_INSTALL_PREFIX_READONLY}/templates")
if(USER_PART)
    add_subdirectory(templates)
endif(USER_PART)

# But generated interceptors are per-kernel.
set(KEDR_COI_KERNEL_INSTALL_PREFIX_INTERCEPTORS
    "${KEDR_COI_INSTALL_PREFIX_READONLY}/interceptors/%kernel%")

set(KEDR_COI_TEST_PREFIX_INTERCEPTORS
    "${CMAKE_BINARY_DIR}/interceptors/")

kernel_make_path(KEDR_COI_INSTALL_MAKE_PREFIX_INTERCEPTORS
    ${KEDR_COI_KERNEL_INSTALL_PREFIX_INTERCEPTORS})

if(KERNEL_PART)
    kernel_part_path(KEDR_COI_INSTALL_PREFIX_INTERCEPTORS
	"${KEDR_COI_KERNEL_INSTALL_PREFIX_INTERCEPTORS}")
    add_subdirectory(interceptors)
endif(KERNEL_PART)

# Examples are common.
if(USER_PART AND NOT CMAKE_CROSSCOMPILING)
    include(example_system)
    add_subdirectory(examples)
endif(USER_PART AND NOT CMAKE_CROSSCOMPILING)

if(USER_PART)
    add_subdirectory(doc)
endif(USER_PART)

if(KERNEL_PART)
    configure_file("${CMAKE_SOURCE_DIR}/config_kernel.h.in" "${CMAKE_BINARY_DIR}/config_kernel.h")
endif(KERNEL_PART)

#######################################################################
option(WITH_TESTING "Whether need to build tests" OFF)
if(WITH_TESTING)
    add_subdirectory(tests)
endif(WITH_TESTING)
#######################################################################

if(KERNEL_PART)
    kbuild_finalize_linking()
endif(KERNEL_PART)
#######################################################################
if(USER_PART_ONLY AND DKMS)
    set(dkms_sources_tree ${KEDR_COI_INSTALL_PREFIX_VAR}/dkms)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/" # Tail '/' means do not create 'sources' directory.
	DESTINATION ${dkms_sources_tree}
	COMPONENT dkms-common
    )
    dkms_install(dkms_common
	SOURCES ${dkms_sources_tree}
	COMPONENT dkms-common
    )
endif(USER_PART_ONLY AND DKMS)
#######################################################################
# CPack support
# Variables set here are described at http://cmake.org/Wiki/CMake:CPackConfiguration.
#
# At this stage - this is a list of supported generators.
set(CPACK_GENERATOR "RPM;DEB;TGZ;TZ")

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY "1")

set(CPACK_PACKAGE_NAME ${KEDR_COI_PACKAGE_NAME})

set(CPACK_PACKAGE_VERSION_MAJOR ${KEDR_COI_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${KEDR_COI_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${KEDR_COI_VERSION_PATCH})

set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Callback Operations Interception for KEDR framework")

# Generator-specific delcarations are here.
set(CPACK_PROJECT_CONFIG_FILE "${PROJECT_SOURCE_DIR}/cpack/CPackOptions.cmake")


# Components-related definitions.
#
# See http://www.cmake.org/Wiki/CMake:Component_Install_With_CPack.
set(CPACK_COMPONENTS_ALL)
if(USER_PART)
    list(APPEND CPACK_COMPONENTS_ALL
# There is not common files for 'core' component, like executables or scripts.
	devel
	docs
    )
    if(BUILD_TESTS)
	list(APPEND CPACK_COMPONENTS_ALL tests)
    endif(BUILD_TESTS)
    if(DKMS)
	list(APPEND CPACK_COMPONENTS_ALL dkms-common)
    endif(DKMS)
endif(USER_PART)
if(KERNEL_PART)
    list(APPEND CPACK_COMPONENTS_ALL
	core-kernel
	devel-kernel
    )
    if(BUILD_TESTS)
	list(APPEND CPACK_COMPONENTS_ALL tests-kernel)
    endif(BUILD_TESTS)
endif(KERNEL_PART)

# There is no group-division for components in the project.
set(CPACK_COMPONENTS_IGNORE_GROUPS "1")

set(CPACK_COMPONENT_DEVEL_DISPLAY_NAME "Development files")
set(CPACK_COMPONENT_DEVEL_DESCRIPTION "Files for build software based on kedr coi.")

set(CPACK_COMPONENT_DOCS_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCS_DESCRIPTION "Documentation for kedr coi.")

set(CPACK_COMPONENT_TESTS_DISPLAY_NAME "Tests")
set(CPACK_COMPONENT_TESTS_DESCRIPTION "Tests for kedr coi.")

set(CPACK_COMPONENT_CORE-KERNEL_DISPLAY_NAME "Main kernel module")
set(CPACK_COMPONENT_CORE-KERNEL_DESCRIPTION "Kernel module which implements main functionality.")

set(CPACK_COMPONENT_DEVEL-KERNEL_DISPLAY_NAME "Development files (kernel-dependent part)")
set(CPACK_COMPONENT_DEVEL-KERNEL_DESCRIPTION "Files for build software based on kedr coi (kernel-dependent part).")

set(CPACK_COMPONENT_TESTS-KERNEL_DISPLAY_NAME "Tests (kernel-dependent part")
set(CPACK_COMPONENT_TESTS-KERNEL_DESCRIPTION "Tests for kedr coi (kernel-dependent part).")


# CPack module is included after all needed variables are set.
include(CPack)
#######################################################################
# TODO: Message should reflect KERNEL_PART_ONLY/ USER_PART_ONLY.
message (STATUS "Configured ${KEDR_COI_PACKAGE_NAME} version ${KEDR_COI_VERSION}")

#######################################################################
