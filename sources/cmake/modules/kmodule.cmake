set(kmodule_this_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(kmodule_test_sources_dir "${CMAKE_SOURCE_DIR}/cmake/kmodule_sources")

set(kmodule_function_map_file "")
if (CMAKE_CROSSCOMPILING)
	if (KEDR_COI_SYSTEM_MAP_FILE)
		set (kmodule_function_map_file "${KEDR_COI_SYSTEM_MAP_FILE}")
	else (KEDR_COI_SYSTEM_MAP_FILE)
# KEDR_COI_SYSTEM_MAP_FILE is not specified, construct the default path 
# to the symbol map file.
		set (kmodule_function_map_file 
	"${KEDR_COI_ROOT_DIR}/boot/System.map-${KBUILD_VERSION_STRING}"
		)
	endif (KEDR_COI_SYSTEM_MAP_FILE)
endif (CMAKE_CROSSCOMPILING)

# kmodule_try_compile(RESULT_VAR bindir srcfile
#           [COMPILE_DEFINITIONS flags ...]
#           [OUTPUT_VARIABLE var])

# Similar to try_module in simplified form, but compile srcfile as
# kernel module, instead of user space program.
#
# 'flags' for COMPILE_DEFINITIONS may be any but without "'" symbols.

function(kmodule_try_compile RESULT_VAR bindir srcfile)
	set(current_scope)
	#set(is_compile_definitions_current "FALSE")
	#set(is_output_var_current "FALSE")
	to_abs_path(src_abs_path "${srcfile}")
	foreach(arg ${ARGN})
		if(arg STREQUAL "COMPILE_DEFINITIONS")
			#set(is_compile_definitions_current "TRUE")
			#set(is_output_var_current "FALSE")
			set(current_scope "COMPILE_DEFINITIONS")
		elseif(arg STREQUAL "OUTPUT_VARIABLE")
			#set(is_compile_definitions_current "FALSE")
			#set(is_output_var_current "TRUE")
			set(current_scope "OUTPUT_VARIABLE")
		elseif(current_scope STREQUAL "COMPILE_DEFINITIONS")
			set(kmodule_cflags "${kmodule_cflags} '${arg}'")
		elseif(current_scope STREQUAL "OUTPUT_VARIABLE")
			set(output_variable "${arg}")
		else(arg STREQUAL "COMPILE_DEFINITIONS")
			message(FATAL_ERROR 
				"Unknown parameter passed to kmodule_try_compile: '${arg}'."
			)
		endif(arg STREQUAL "COMPILE_DEFINITIONS")
	endforeach(arg ${ARGN})
	# Collect parameters to try_compile() function
	set(try_compile_params
		result_tmp # Result variable(temporary)
		"${bindir}" # Binary directory
		"${kmodule_this_module_dir}/kmodule_files" # Source directory
		"kmodule_try_compile_target" # Project name
         CMAKE_FLAGS # Flags to CMake:
			"-DSRC_FILE:path=${src_abs_path}" # Path to real source file
			"-DKERNELDIR=${KBUILD_BUILD_DIR}" # Kernel build directory
			"-DKEDR_COI_ARCH=${KEDR_COI_ARCH}" # Arhictecture
			"-DKEDR_COI_CROSS_COMPILE=${KEDR_COI_CROSS_COMPILE}" # Cross compilation
		)

	if(DEFINED kmodule_cflags)
		list(APPEND try_compile_params
			"-Dkmodule_flags:STRING=${kmodule_cflags}" # Parameters for compiler
			)
	endif(DEFINED kmodule_cflags)

	if(DEFINED output_variable)
		list(APPEND try_compile_params
			OUTPUT_VARIABLE output_tmp # Output variable(temporary)
		)
	endif(DEFINED output_variable)
	try_compile(${try_compile_params})
	
	if(DEFINED output_variable)
		# Set output variable for the caller
		set("${output_variable}" "${output_tmp}" PARENT_SCOPE)
	endif(DEFINED output_variable)
	# Set result variable for the caller
	set("${RESULT_VAR}" "${result_tmp}" PARENT_SCOPE)
endfunction(kmodule_try_compile RESULT_VAR bindir srcfile)


############################################################################
# Utility macros to check for particular features. If the particular feature
# is supported, the macros will set the corresponding variable to TRUE, 
# otherwise - to FALSE (the name of variable is mentioned in the comments 
# for the macro). 
############################################################################

# Check if the system has everything necessary to build at least simple
# kernel modules. 
# The macro sets variable 'MODULE_BUILD_SUPPORTED'.
macro(check_module_build)
	set(check_module_build_message 
		"Checking if kernel modules can be built on this system"
	)
	message(STATUS "${check_module_build_message}")
	if (DEFINED MODULE_BUILD_SUPPORTED)
		set(check_module_build_message 
"${check_module_build_message} [cached] - ${MODULE_BUILD_SUPPORTED}"
		)
	else (DEFINED MODULE_BUILD_SUPPORTED)
		kmodule_try_compile(module_build_supported_impl 
			"${CMAKE_BINARY_DIR}/check_module_build"
			"${kmodule_test_sources_dir}/check_module_build/module.c"
		)
		if (module_build_supported_impl)
			set(MODULE_BUILD_SUPPORTED "yes" CACHE INTERNAL
				"Can kernel modules be built on this system?"
			)
		else (module_build_supported_impl)
			set(MODULE_BUILD_SUPPORTED "no")
			message(FATAL_ERROR 
				"Kernel modules cannot be built on this system"
			)
		endif (module_build_supported_impl)
				
		set(check_module_build_message 
"${check_module_build_message} - ${MODULE_BUILD_SUPPORTED}"
		)
	endif (DEFINED MODULE_BUILD_SUPPORTED)
	message(STATUS "${check_module_build_message}")
endmacro(check_module_build)

# Check if the version of the kernel is acceptable
# The macro sets variable 'KERNEL_VERSION_OK'.
macro(check_kernel_version kversion_major kversion_minor kversion_micro)
	set(check_kernel_version_string 
"${kversion_major}.${kversion_minor}.${kversion_micro}"
	)
	set(check_kernel_version_message 
"Checking if the kernel version is ${check_kernel_version_string} or newer"
	)
	message(STATUS "${check_kernel_version_message}")
	if (DEFINED KERNEL_VERSION_OK)
		set(check_kernel_version_message 
"${check_kernel_version_message} [cached] - ${KERNEL_VERSION_OK}"
		)
	else (DEFINED KERNEL_VERSION_OK)
		string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" 
			real_kernel_version_string
			"${KBUILD_VERSION_STRING}"
		)

		if (real_kernel_version_string VERSION_LESS check_kernel_version_string)
			set(KERNEL_VERSION_OK "no")
			message(FATAL_ERROR 
"Kernel version is ${real_kernel_version_string} but ${check_kernel_version_string} or newer is required."
			)
		else ()
			set(KERNEL_VERSION_OK "yes" CACHE INTERNAL
				"Is kernel version high enough?"
			)
		endif ()
				
		set(check_kernel_version_message 
"${check_kernel_version_message} - ${KERNEL_VERSION_OK}"
		)
	endif (DEFINED KERNEL_VERSION_OK)
	message(STATUS "${check_kernel_version_message}")
endmacro(check_kernel_version kversion_major kversion_minor kversion_micro)
############################################################################

# kmodule_is_operation_exist(RESULT_VAR bindir
#     operations_include_dir operations_struct operation_name [operation_type]
#     )
#
# Determine, whether given 'operations_struct' has operation field with name
# 'operation_name'. If 'operation_type' is set, verify that type of this field
# coincide with 'operation_type'
#
# 'operations_include_dir' should contain header file "operations_include.h",
# which define 'operations_struct'.
#
# If operation exist and has expected type, RESULT_VAR will be set to "FOUND".
# Otherwise it will be set to "NOTFOUND".
# RESULT_VAR will be cached in any case.
#
# 'bindir' will be used for compilation attempt.

function(kmodule_is_operation_exist RESULT_VAR bindir operations_include_dir operations_struct_type operation_name)
	set(status_message "Looking for '${operation_name}' callback operation in the '${operations_struct_type}'")

	list(LENGTH ARGN rest_params_n)
	if(rest_params_n GREATER 0)
		list(GET ARGN 0 operation_type)
		set(status_message "${status_message}(type ${operation_type})")
	endif(rest_params_n GREATER 0)


	if(NOT DEFINED ${RESULT_VAR})
		set(kmodule_try_compile_params result_tmp
			"${bindir}"	# Binary directory
			"${kmodule_test_sources_dir}/check_operation/check_operation.c" # Source file
			COMPILE_DEFINITIONS # Definitions for compiler
				"-D" "OPERATIONS_STRUCT_TYPE=${operations_struct_type}"
				"-D" "OPERATION_NAME=${operation_name}"
				"-I" "${operations_include_dir}")
		
		if(operation_type)
			LIST(APPEND kmodule_try_compile_params
				"-D" "OPERATION_TYPE=${operation_type}")
		endif(operation_type)

		kmodule_try_compile(${kmodule_try_compile_params})

		if(result_tmp)
			set(result_tmp "FOUND")
		else(result_tmp)
			set(result_tmp "NOTFOUND")
		endif(result_tmp)
	
		set("${RESULT_VAR}" "${result_tmp}" CACHE INTERNAL "Whether operation ${operation_name} of type '${operation_type}' exists in ${operations_struct_type}.")
	else(NOT DEFINED ${RESULT_VAR})
		set(status_cached " [cached]")
	endif(NOT DEFINED ${RESULT_VAR})
	
	if(${RESULT_VAR})
		set(status_result "found")
	else(${RESULT_VAR})
		set(status_result "not found")
	endif(${RESULT_VAR})
	set(status_message "${status_message} - ${status_result}${status_cached}")
	message(STATUS "${status_message}")
endfunction(kmodule_is_operation_exist RESULT_VAR bindir operations_include_dir operations_struct_type operation_name)
	

