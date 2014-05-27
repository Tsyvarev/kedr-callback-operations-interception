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

# kmodule_try_compile_operation(RESULT_VAR bindir
#     header_file operations_struct operation_name [operation_type]
# 	  )
#
# Try to compile kernel module, which assign given operation.
# IIf operation_type is given, use this type when assign operation.
# @header_file should contain definitions for make @operations_struct
# available.
#
# Result of compilation is stored(not cached!) in @RESULT_VAR variable.
function(kmodule_try_compile_operation RESULT_VAR bindir
     header_file operations_struct operation_name)

     set(operation_type_param)
     foreach(arg ${ARGN})
		set(operation_type_param "-DOPERATION_TYPE:string=${arg}")
     endforeach(arg ${ARGN})
     
     try_compile(result_tmp # Result variable(temporary)
		"${bindir}" # Binary directory
		"${kmodule_this_module_dir}/try_compile_operation" # Source directory
		"kmodule_try_compile_operation" # Project name
         CMAKE_FLAGS # Flags to CMake:
			"-DKERNELDIR:path=${KBUILD_BUILD_DIR}" # Kernel build directory
			"-DARCH:string=${KEDR_COI_ARCH}" # Arhictecture
			"-DCROSS_COMPILE:string=${KEDR_COI_CROSS_COMPILE}" # Cross compilation
			"-DHEADER_FILE:path=${header_file}"
			"-DOPERATIONS_STRUCT:string=${operations_struct}"
			"-DOPERATION_NAME:string=${operation_name}"
			"${operation_type_param}"
		)
     set(${RESULT_VAR} ${result_tmp} PARENT_SCOPE)
endfunction(kmodule_try_compile_operation RESULT_VAR bindir
     header_file operations_struct operation_name)


# Setup data for future kmodule_configure_operations() call(s).
#
# @operations_struct_id should identify operations structure and
# is used for cache variables naming, binary directory naming and so.
function(kmodule_configure_operations_setup operations_struct_id
	header_file operations_struct)
	set(_kmodule_operations_struct_id ${operations_struct_id} PARENT_SCOPE)
	set(_kmodule_header_file ${header_file} PARENT_SCOPE)
	set(_kmodule_operations_struct ${operations_struct} PARENT_SCOPE)
endfunction(kmodule_configure_operations_setup operations_struct_id
	header_file operations_struct)


macro(_kmodule_configure_on_end_typed)
	set(cache_var "${_kmodule_operations_struct_id}_has_${operation_name}_${operation_type_suffix}")
	if(NOT DEFINED ${cache_var})
		check_try()
		kmodule_try_compile_operation(result_tmp
			"${CMAKE_BINARY_DIR}/try_operation/${_kmodule_operations_struct_id}/${operation_name}/${operation_type_suffix}"
			${_kmodule_header_file}
			${_kmodule_operations_struct}
			${operation_name}
			${operation_type}
		)
		set(${cache_var} ${result_tmp} CACHE INTERNAL
			"${_kmodule_operations_struct} has operation '${operation_name}' with type ${operation_type}")
	endif(NOT DEFINED ${cache_var})
	if(${cache_var})
		set(is_type_determined 1)
	endif(${cache_var})
endmacro(_kmodule_configure_on_end_typed)

macro(_kmodule_configure_on_end_operation)
	if(mode STREQUAL "NOT_CHECKED")
		list(APPEND output_list_tmp "${operation_name}.yaml")
	elseif(is_typed)
		if(is_type_determined)
			check_end("${operation_type}")
			list(APPEND output_list_tmp "${operation_name}_${operation_type_suffix}.yaml")
		else(is_type_determined)
			check_end("not found")
			if(mode STREQUAL "REQUIRED")
				message(FATAL_ERROR "None of types is suited for operation ${operation_name} in ${_kmodule_operations_struct}")
			endif(mode STREQUAL "REQUIRED")
		endif(is_type_determined)
	else(mode STREQUAL "NOT_CHECKED")
		set(cache_var "${_kmodule_operations_struct_id}_has_${operation_name}")
		if(NOT DEFINED ${cache_var})
			check_try()
			kmodule_try_compile_operation(result_tmp
				"${CMAKE_BINARY_DIR}/try_operation/${_kmodule_operations_struct_id}/${operation_name}"
				${_kmodule_header_file}
				${_kmodule_operations_struct}
				${operation_name}
			)
			set(${cache_var} ${result_tmp} CACHE INTERNAL
				"${_kmodule_operations_struct} has operation '${operation_name}'")
		endif(NOT DEFINED ${cache_var})
		if(${cache_var})
			check_end("found")
			list(APPEND output_list_tmp "${operation_name}.yaml")
		else(${cache_var})
			check_end("not found")
			if(mode STREQUAL "REQUIRED")
				message(FATAL_ERROR "None of types is suited for operation ${operation_name} in ${_kmodule_operations_struct}")
			endif(mode STREQUAL "REQUIRED")
		endif(${cache_var})
	endif(mode STREQUAL "NOT_CHECKED")
endmacro(_kmodule_configure_on_end_operation)

# kmodule_configure_operations(output_list
#	operation_name [TYPED suffix type]*
#	| REQUIRED | OPTIONAL | NOT_CHECKED ...
# )
#
# Configure list of yaml-files which describe operations.
#
# For each operation @operation_name, which exists,
#	"@operation_name.yaml"
# record is added to the output_list.
# IIf TYPED sequence is specified just after @operation_name,
# all @type's are checked for being real operation's type. If found,
#   "@{operation_name}_@{suffix}.yaml"
# record is added to the output list.
#
# Keywords REQUIRED, OPTIONAL and NOT_CHECKED determine requirements
# on operation existing status for future operations requested
# (until another keyword is specified):
#
#  - every operation in REQUIRED section should exist.
#  - operations in OPTIONAL section is allowed to miss on given system.
#  - every operation in NOT_CHECKED section should exist, but no
#		checks is performed on it.
# 
function(kmodule_configure_operations output_list)
	set(output_list_tmp)
	set(mode "NOT_CHECKED")
	# Name of the active @operation_name, if issued.
	set(operation_name)
	# "" or "suffix_expected" or "type_expected"
	set(typed_state)
	# Whether one or more 'TYPED' sequences has found for operation.
	set(is_typed)
	# Whether type is determined for operation.
	# IIf true, @operation_type and @operation_type_suffix contains corresponded values.
	set(is_type_determined)
	foreach(arg ${ARGN})
		if(operation_name)
			if(typed_state STREQUAL "suffix_expected")
				if(NOT is_type_determined)
					set(operation_type_suffix ${arg})
				endif(NOT is_type_determined)
				set(typed_state "type_expected")
			elseif(typed_state STREQUAL "type_expected")
				if(NOT is_type_determined)
					set(operation_type ${arg})
					_kmodule_configure_on_end_typed()
				endif(NOT is_type_determined)
				set(typed_state)
			else(typed_state STREQUAL "suffix_expected")
				if(arg STREQUAL "TYPED")
					if(mode STREQUAL "NOT_CHECKED")
						message(FATAL_ERROR "'TYPED' sequences shouldn't be used for 'NOT_CHECKED' mode")
					endif(mode STREQUAL "NOT_CHECKED")
					set(is_typed "1")
					set(typed_state "suffix_expected")
				else(arg STREQUAL "TYPED")
					_kmodule_configure_on_end_operation()
					set(operation_name)
				endif(arg STREQUAL "TYPED")
			endif(typed_state STREQUAL "suffix_expected")
		endif(operation_name)
		if(NOT operation_name)
			if(arg STREQUAL "NOT_CHECKED")
				set(mode "NOT_CHECKED")
			elseif(arg STREQUAL "REQUIRED")
				set(mode "REQUIRED")
			elseif(arg STREQUAL "OPTIONAL")
				set(mode "OPTIONAL")
			else(arg STREQUAL "NOT_CHECKED")
				set(operation_name ${arg})
				if(NOT mode STREQUAL "NOT_CHECKED")
					check_begin("Looking for '${operation_name}' callback operation in the '${_kmodule_operations_struct}'")
				endif(NOT mode STREQUAL "NOT_CHECKED")
				set(is_typed)
				set(is_type_determined)
			endif(arg STREQUAL "NOT_CHECKED")
		endif(NOT operation_name)
	endforeach(arg ${ARGN})
	if(operation_name)
		_kmodule_configure_on_end_operation()
		set(operation_name)
	endif(operation_name)
	set(${output_list} ${output_list_tmp} PARENT_SCOPE)
endfunction(kmodule_configure_operations output_list)

# Check if hlist_for_each_entry*() macros accept only 'type *pos' argument
# rather than both 'type *tpos' and 'hlist_node *pos' as the loop cursors.
# The macro sets variable 'HLIST_FOR_EACH_ENTRY_POS_ONLY'.
macro(check_hlist_for_each_entry)
	set(check_hlist_for_each_entry_message
"Checking the signatures of hlist_for_each_entry*() macros"
	)
	message(STATUS "${check_hlist_for_each_entry_message}")
	if (DEFINED HLIST_FOR_EACH_ENTRY_POS_ONLY)
		set(check_hlist_for_each_entry_message
"${check_hlist_for_each_entry_message} [cached] - done"
		)
	else ()
		kmodule_try_compile(pos_only_impl
			"${CMAKE_BINARY_DIR}/check_hlist_for_each_entry"
			"${kmodule_test_sources_dir}/check_hlist_for_each_entry/module.c"
		)
		if (pos_only_impl)
			set(HLIST_FOR_EACH_ENTRY_POS_ONLY "yes" CACHE INTERNAL
	"Do hlist_for_each_entry*() macros have only 'type *pos' to use as a loop cursor?"
			)
		else ()
			set(HLIST_FOR_EACH_ENTRY_POS_ONLY "no" CACHE INTERNAL
	"Do hlist_for_each_entry*() macros have only 'type *pos' to use as a loop cursor?"
			)
		endif (pos_only_impl)

		set(check_hlist_for_each_entry_message
			"${check_hlist_for_each_entry_message} - done"
		)
	endif (DEFINED HLIST_FOR_EACH_ENTRY_POS_ONLY)
	message(STATUS "${check_hlist_for_each_entry_message}")
endmacro(check_hlist_for_each_entry)
