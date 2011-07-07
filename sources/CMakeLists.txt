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
set(KEDR_COI_VERSION_MINOR 0)
set(KEDR_COI_VERSION_MICRO 1)
set(KEDR_COI_VERSION_SUFFIX "-devel" CACHE STRING
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
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif ()
message (STATUS "Build type is \"${CMAKE_BUILD_TYPE}\"")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    kbuild_add_definitions(
        "-DKEDR_COI_DEBUG"
    )
endif()

#######################################################################
# Perform basic checks

# Check if we use kernel version 2.6.31 or newer
check_kernel_version(2 6 31)

# Check if kernel modules can be built on this system
check_module_build()

#######################################################################

# kedr_gen (this time - for building KEDR COI itself)
# Also need to set ${KEDR_GEN_TOOL} as the path to "kedr_gen". It can be used to 
# execute "kedr_gen" tool during the build of payload modules, etc.

set(KEDR_GEN CACHE FILEPATH "If set, it is a path to kedr_gen tool for use while building KEDR COI")

if (KEDR_GEN)
	# Check if the tool exists at the path specified.
	execute_process (
	    COMMAND ${KEDR_GEN} 
	    RESULT_VARIABLE kedr_gen_exec_result
		OUTPUT_QUIET
	)
	if (NOT kedr_gen_exec_result EQUAL 0)
	    message (FATAL_ERROR 
	"${KEDR_GEN} does not exist or cannot be executed."
	    )
	endif ()
	
    set (KEDR_GEN_TOOL ${KEDR_GEN})
    
    # ${KEDR_GEN_TOOL_INSTALLED} is the path to "kedr_gen" installation
    # for outer use. Because KEDR gen is already installed, we will use existent
    # installation.
    set (KEDR_GEN_TOOL_INSTALLED ${KEDR_GEN})

	message (STATUS "\"kedr_gen\" tool: ${KEDR_GEN}")

else (KEDR_GEN)
# kedr_gen is built here explicitly and installed to a temporary location.
# This is only necessary to build KEDR itself.
# This should be done before the first add_subdirectory() command.

# The temporary install directory must be persistent and it must not be 
# KEDR_COI_INSTALL_PREFIX_TEMP because it has nothing to do with the 
# installation of KEDR COI: it is used when KEDR COI is being built only.

	message (STATUS "Creating \"kedr_gen\"")
	set (KEDR_GEN_TEMP_INSTALL "${CMAKE_CURRENT_BINARY_DIR}/kedr_gen.install")
	set (KEDR_GEN_TEMP_BUILD "${CMAKE_CURRENT_BINARY_DIR}/kedr_gen.build")

# The temporary build directory should be removed during 'make clean'
# Note that the temporary install directory containing 'kedr_gen' must 
# survive as kedr_gen will be necessary if the user wants to rebuild KEDR 
# after 'make clean' without reconfiguring.
	set_property (DIRECTORY APPEND PROPERTY 
	    ADDITIONAL_MAKE_CLEAN_FILES kedr_gen.build
	)

# Remove the old temporary build and install directories first and 
# (re)create them.

	execute_process (
	    COMMAND mkdir -p "${KEDR_GEN_TEMP_BUILD}" 
	    RESULT_VARIABLE kedr_gen_result
	)
	if (NOT kedr_gen_result EQUAL 0)
	    message (FATAL_ERROR 
	"Failed to create directory ${KEDR_GEN_TEMP_BUILD}"
	    )
	endif ()

	execute_process (
	    COMMAND mkdir -p "${KEDR_GEN_TEMP_INSTALL}" 
	    RESULT_VARIABLE kedr_gen_result
	)
	if (NOT kedr_gen_result EQUAL 0)
	    message (FATAL_ERROR 
	"Failed to create directory ${KEDR_GEN_TEMP_BUILD}"
	    )
	endif ()

	# Configure kedr_gen
	execute_process (
	    COMMAND ${CMAKE_COMMAND} 
	        -DCMAKE_INSTALL_PREFIX=${KEDR_GEN_TEMP_INSTALL}
	        -DCMAKE_BUILD_TYPE=Release
	        -DKEDR_GEN_INSTALL_PREFIX=${KEDR_GEN_TEMP_INSTALL}
	        "${CMAKE_SOURCE_DIR}/tools/kedr_gen/src"
	    WORKING_DIRECTORY ${KEDR_GEN_TEMP_BUILD}
	    RESULT_VARIABLE kedr_gen_result
	    OUTPUT_VARIABLE kedr_gen_output
	    ERROR_VARIABLE  kedr_gen_output
	)
	if (NOT kedr_gen_result EQUAL 0)
	    message ("Failed to configure \"kedr_gen\".")
	    message ("CMake output:\n${kedr_gen_output}\n")
	    message ("CMake result:\n${kedr_gen_result}\n")
	    message (FATAL_ERROR "Unable to build \"kedr_gen\", aborting.")
	endif ()

	# Build kedr_gen
	execute_process (
	    COMMAND make
	    WORKING_DIRECTORY ${KEDR_GEN_TEMP_BUILD}
	    RESULT_VARIABLE kedr_gen_result
	    OUTPUT_VARIABLE kedr_gen_output
	    ERROR_VARIABLE  kedr_gen_output
	)
	if (NOT kedr_gen_result EQUAL 0)
	    message ("Failed to build \"kedr_gen\".")
	    message ("Make output:\n${kedr_gen_output}\n")
	    message ("Make result:\n${kedr_gen_result}\n")
	    message (FATAL_ERROR "Unable to build \"kedr_gen\", aborting.")
	endif ()

	# Install kedr_gen
	execute_process (
	    COMMAND make install
	    WORKING_DIRECTORY ${KEDR_GEN_TEMP_BUILD}
	    RESULT_VARIABLE kedr_gen_result
	    OUTPUT_VARIABLE kedr_gen_output
	    ERROR_VARIABLE  kedr_gen_output
	)
	if (NOT kedr_gen_result EQUAL 0)
	    message ("Failed to install \"kedr_gen\" to ${KEDR_GEN_INSTALL_PREFIX}.")
	    message ("Make output:\n${kedr_gen_output}\n")
	    message ("Make result:\n${kedr_gen_result}\n")
	    message (FATAL_ERROR "Unable to install \"kedr_gen\", aborting.")
	endif ()

# ${KEDR_GEN_TOOL} is the path to "kedr_gen". The path can be used to call
# "kedr_gen" tool during the build of payload modules, etc.
	set (KEDR_GEN_TOOL ${KEDR_GEN_TEMP_INSTALL}/kedr_gen)
endif (KEDR_GEN)

# Top directory with kedr_gen templates for different purposes.
set(KEDR_GEN_COI_TEMPLATES_DIR "${CMAKE_SOURCE_DIR}/templates/") 
	message (STATUS "Creating \"kedr_gen\" - done")
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

#######################################################################
message (STATUS "Configured ${KEDR_COI_PACKAGE_NAME} version ${KEDR_COI_VERSION}")

#######################################################################
