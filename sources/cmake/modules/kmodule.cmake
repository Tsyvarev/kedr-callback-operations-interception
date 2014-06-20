# Project-specific kernel module-related operations.

set(kmodule_this_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(kmodule_test_sources_dir "${CMAKE_SOURCE_DIR}/cmake/kmodule_sources")

# Check that modules can really be built
function(check_module_build)
	check_begin("Checking if kernel modules can be built on this system")

    if (NOT MODULE_BUILD_SUPPORTED)
        check_try()
        kbuild_try_compile(_module_build_result
            "${CMAKE_BINARY_DIR}/check_module_build"
            "${kmodule_test_sources_dir}/check_module_build/module.c"
        )
        # Transform result to "yes"/"no" (instead of "TRUE"/"FALSE").
        if(_module_build_result)
            set(_module_build_result "yes")
        else(_module_build_result)
            set(_module_build_result "no")
        endif(_module_build_result)
        
        set(MODULE_BUILD_SUPPORTED "${_module_build_result}" CACHE INTERNAL
            "Can kernel modules be built on this system?"
        )
    endif (NOT MODULE_BUILD_SUPPORTED)
	check_end("${MODULE_BUILD_SUPPORTED}")

    if(NOT MODULE_BUILD_SUPPORTED)
        message(FATAL_ERROR "Kernel modules cannot be built with given KBuild system.")
    endif(NOT MODULE_BUILD_SUPPORTED)
endfunction(check_module_build)
############################################################################

# kmodule_try_compile_operation(RESULT_VAR bindir
#     header_file operations_struct operation_name [operation_type]
#       )
#
# Try to compile kernel module, which assign given operation.
# IIf operation_type is given, use this type when assign operation.
# @header_file should contain definitions for make @operations_struct
# available.
#
# Result of compilation is stored(not cached!) in @RESULT_VAR variable.
function(kmodule_try_compile_operation RESULT_VAR bindir
    header_file operations_struct operation_name)

    set(operation_type)
    foreach(arg ${ARGN})
		set(operation_type "${arg}")
    endforeach(arg ${ARGN})

    if(operation_type)
        set(configured_source_name "try_compile_operation_typed.c.in")
    else(operation_type)
        set(configured_source_name "try_compile_operation.c.in")
    endif(operation_type)
    
    set(source_file "${bindir}/try_compile_operation.c")
    
    if(NOT IS_ABSOLUTE ${header_file})
        message(FATAL_ERROR "<header_file> parameter should be absolute path")
    endif(NOT IS_ABSOLUTE ${header_file})

    get_filename_component(include_dir "${header_file}" PATH)
    get_filename_component(include_file "${header_file}" NAME)
    
    configure_file("${kmodule_this_module_dir}/try_compile_operation/${configured_source_name}"
        ${source_file})

    kbuild_try_compile(result_tmp # Result variable(temporary)
        "${bindir}" # Binary directory
        "${source_file}"
        CMAKE_FLAGS "-DKBUILD_INCLUDE_DIRECTORIES=${include_dir}"
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
#    operation_name [TYPED suffix type]*
#    | REQUIRED | OPTIONAL | NOT_CHECKED ...
# )
#
# Configure list of yaml-files which describe operations.
#
# For each operation @operation_name, which exists,
#    "@operation_name.yaml"
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
#        checks is performed on it.
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
        kbuild_try_compile(pos_only_impl
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
