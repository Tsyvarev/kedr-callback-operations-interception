cmake_minimum_required(VERSION 2.6)

enable_language(C)
enable_language(CXX)

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
# ARCH and CROSS_COMPILE variables that can be used when building 
# kernel modules. It is convenient to have them as environment variables
# because Makefiles will be able to use them directly.
if (NOT KEDR_COI_ARCH)
	set (KEDR_COI_ARCH "x86")
endif ()

set (ENV{KEDR_COI_ARCH} "${KEDR_COI_ARCH}")
set (ENV{KEDR_COI_CROSS_COMPILE} "${KEDR_COI_CROSS_COMPILE}")

#######################################################################
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
find_package(Kbuild)

include(cmake_useful)
include(kbuild_system)
include(kmodule)

include(path_prefixes)

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
#kedr_coi_install_kmodule(kmodule_name)
function(kedr_coi_install_kmodule kmodule_name)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${kmodule_name}.ko"
			DESTINATION "${KEDR_COI_INSTALL_PREFIX_KMODULE}")
endfunction(kedr_coi_install_kmodule kmodule_name)
#kedr_coi_install_symvers(kmodule_name)
function(kedr_coi_install_symvers kmodule_name)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/Module.symvers"
			DESTINATION "${KEDR_COI_INSTALL_PREFIX_KSYMVERS}"
			RENAME "${kmodule_name}.symvers")
endfunction(kedr_coi_install_symvers kmodule_name)

#######################################################################
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

add_custom_target (uninstall_files
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
)

add_custom_target (uninstall)

# During uninstall process, the files should be removed first, then 
# the directories.
# 'uninstall_dirs' target is defined in cmake/modules/path_prefixes.cmake.
add_dependencies (uninstall_dirs uninstall_files)
add_dependencies (uninstall uninstall_dirs)

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
    kbuild_add_definitions(
        "-Wall -Wextra -Wno-unused-parameter -DKEDR_COI_DEBUG -g"
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    kbuild_add_definitions(
        "-Wall"
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    kbuild_add_definitions(
        "-Wall -g"
    )
else (CMAKE_BUILD_TYPE STREQUAL "Debug")
	message(FATAL_ERROR "Incorrect build type: ${CMAKE_BUILD_TYPE}. Should be one of 'Debug', 'Release' or 'RelWithDebInfo'")
endif()

#######################################################################
# Perform basic checks

# Check if we use kernel version 2.6.31 or newer
check_kernel_version(2 6 31)

# Check if kernel modules can be built on this system
check_module_build()

# Determine way of hlist_for_each_* macros usage
check_hlist_for_each_entry()
#######################################################################
set(JY_TOOL "python" "${CMAKE_CURRENT_SOURCE_DIR}/tools/jinja2-yaml/jinja2-yaml.py"
	CACHE INTERNAL "jinja2-yaml tool for build KEDR COI itself")

# Top directory with jinja2 templates for different purposes.
set(COI_TEMPLATES_DIR "${CMAKE_SOURCE_DIR}/templates/") 
#######################################################################

add_subdirectory(core)

add_subdirectory(include)

add_subdirectory(templates)

add_subdirectory(interceptors)

add_subdirectory(tools)

if (NOT CMAKE_CROSSCOMPILING)
	# Examples
	add_subdirectory(examples)
endif (NOT CMAKE_CROSSCOMPILING)

add_subdirectory(doc)

configure_file("${CMAKE_SOURCE_DIR}/config.h.in" "${CMAKE_BINARY_DIR}/config.h")
#######################################################################
message (STATUS "Configured ${KEDR_COI_PACKAGE_NAME} version ${KEDR_COI_VERSION}")

#######################################################################
