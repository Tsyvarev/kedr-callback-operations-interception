# The path where the examples will be installed
set (KEDR_COI_EXAMPLE_PATH "${KEDR_COI_INSTALL_PREFIX_EXAMPLES}")

define_property(TARGET PROPERTY "example_files"
    BRIEF_DOCS "List of files for 'example' target"
    FULL_DOCS "List of files for 'example' target"
)

define_property(TARGET PROPERTY "example_sources"
    BRIEF_DOCS "List of sources files for 'example' target"
    FULL_DOCS "List of source files for 'example' target"
)


# add_example(name files ...)
# Add example as a set of files.
# Currently only filenames are supported in @files list.
# Each file is taken from source directory, if exists, or from binary directory.
# Original file for 'makefile' should be named 'example_makefile'.
function(add_example name file)
    set(files ${file} ${ARGN})
    # List of full names of source files
    set(source_files_full)
    foreach(f ${files})
        set(source_real ${f})
        if(source_real STREQUAL "makefile")
            set(source_real "example_makefile")
        endif(source_real STREQUAL "makefile")
        to_abs_path(source_file_full ${source_real})
        list(APPEND source_files_full ${source_file_full})
    endforeach(f ${files})
    add_custom_target(${name} ALL
        DEPENDS ${source_files_full})
    set_property(TARGET ${name} PROPERTY "example_files" ${files})
    set_property(TARGET ${name} PROPERTY "example_sources" ${source_files_full})
endfunction(add_example name files)

# TODO: Way for set properties for example files(e.g., executable).

# Install example.
# All files, belonged to the example, are installed into given directory.
# Relative directory is assumed to be subdirectory of ${KEDR_COI_EXAMPLE_PATH}.
#
# @name should be same as passed to add_example()
function(install_example name dir)
    string(REGEX MATCH "/" is_dir_abs ${dir})
    if(is_dir_abs)
        set(dir_abs ${dir})
    else(is_dir_abs)
        set(dir_abs ${KEDR_COI_EXAMPLE_PATH}/${dir})
    endif(is_dir_abs)
    
    get_property(example_sources TARGET ${name} PROPERTY "example_sources")
    get_property(example_files TARGET ${name} PROPERTY "example_files")
    list(LENGTH example_files n_files)
    set(index 0)
    while(index LESS ${n_files})
        list(GET example_sources ${index} source)
        list(GET example_files ${index} target)
        install(FILES ${source}
            DESTINATION ${dir_abs}
            RENAME ${target})
        math(EXPR index "${index} + 1")
    endwhile(index LESS ${n_files})
endfunction(install_example name dir)

# test_add_example_copy(name orig_name dir [replace_files])
#
# Create copy of example for testing purposes.
#
# @name - name of the target
# @orig_name - name of example to be copied.
# @dir - directory for install all example's files.
#   If does not exist, it will be created automatically.
#   Relative paths are counted from binary dir.
#   
# @replace_files - files in the original example to be replaced in copy.
#   Rules for that files are same, as in add_example() function.
function(test_add_example_copy name orig_name dir)
    set(replace_files ${ARGN})
    string(REGEX MATCH "/" is_dir_abs ${dir})
    if(is_dir_abs)
        set(dir_abs ${dir})
    else(is_dir_abs)
        set(dir_abs ${CMAKE_CURRENT_BINARY_DIR}/${dir})
    endif(is_dir_abs)
    
    set(target_files)
    
    get_property(example_sources TARGET ${orig_name} PROPERTY "example_sources")
    get_property(example_files TARGET ${orig_name} PROPERTY "example_files")
    list(LENGTH example_files n_files)
    set(index 0)
    while(index LESS ${n_files})
        list(GET example_files ${index} target)
        set(target_file "${dir_abs}/${target}")
        
        list(APPEND target_files ${target_file})

        list(FIND replace_files ${target} replace_index)
        if(replace_index EQUAL "-1")
            list(GET example_sources ${index} source)
        else(replace_index EQUAL "-1")
            set(f ${target})
            if(f STREQUAL "makefile")
                set(f "example_makefile")
            endif(f STREQUAL "makefile")
            to_abs_path(source ${f})
        endif(replace_index EQUAL "-1")

        rule_copy_file(${target_file} ${source})

        math(EXPR index "${index} + 1")
    endwhile(index LESS ${n_files})
    add_custom_target(${name}
        DEPENDS ${dir_abs} ${target_files})
    add_custom_command(OUTPUT ${dir_abs}
        COMMAND mkdir -p ${dir_abs})
    kedr_coi_test_add_target(${name})
    
    set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${dir_abs}")
endfunction(test_add_example_copy name orig_name dir)

# example_export(TARGETS name [...] FILE <filename>)
#
# Similar to export() command, but works with example targets.
function(example_export)
    CMAKE_PARSE_ARGUMENTS(example_export "" "FILE" "TARGETS" ${ARGN})
    if(NOT example_export_TARGETS)
        message(FATAL_ERROR "example_export: At least one target should be given.")
    endif(NOT example_export_TARGETS)
    if(NOT example_export_FILE)
        message(FATAL_ERROR "example_export: FILE <filename> option missing.")
    endif(NOT example_export_FILE)
    if(example_export_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "example_export: Unparsed arguments ${example_export_UNPARSED_ARGUMENTS}.")
    endif(example_export_UNPARSED_ARGUMENTS)
    
    # Check extension of <filename>, like standard export().
    string(REGEX MATCH ".cmake$" cmake_suffix ${example_export_FILE})
    if(NOT cmake_suffix)
        message(SEND_ERROR "example_export: FILE option given filename\n""${example_export_FILE}""\n which does not have an extension of "".cmake"".")
    endif(NOT cmake_suffix)
    
    set(file_content)
    
    foreach(target ${example_export_TARGETS})
        set(file_content "${file_content}add_custom_target(""${target}"")\n")
        get_property(example_files TARGET "${target}" PROPERTY "example_files")
        set(file_content "${file_content}set_property(TARGET ""${target}"" PROPERTY ""example_files"" ${example_files})\n")
        get_property(example_sources TARGET "${target}" PROPERTY "example_sources")
        set(file_content "${file_content}set_property(TARGET ""${target}"" PROPERTY ""example_sources"" ${example_sources})\n")
    endforeach(target ${example_export_TARGETS})
    write_or_update_file(${example_export_FILE} "${file_content}")
endfunction(example_export)

add_subdirectory(read_counter)

# This example uses KEDR framework.
if(KEDR_INSTALL_DIR)
    add_subdirectory(file_access_counter)
endif(KEDR_INSTALL_DIR)
