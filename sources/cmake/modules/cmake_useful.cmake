# Create rule for obtain one file by copying another one
function(rule_copy_file target_file source_file)
    add_custom_command(OUTPUT ${target_file}
                    COMMAND cp -p ${source_file} ${target_file}
                    DEPENDS ${source_file}
                    )
endfunction(rule_copy_file target_file source_file)

# rule_copy_source([source_dir] file ...)
#
# Create rule for obtain file(s) in binary tree by copiing it from source tree.
#
# Files are given relative to ${source_dir}, if it is set, or
# relative to ${CMAKE_CURRENT_SOURCE_DIR}.
#
# Files will be copied into ${CMAKE_CURRENT_BINARY_DIR} with same
# relative paths.
#
# ${source_dir} should be absolute path(that is, starts from '/').
# Otherwise first argument is treated as first file to copy.

function(rule_copy_source file)
    string(REGEX MATCH "^/" is_abs_path ${file})
	if(is_abs_path)
		set(source_dir ${file})
		set(files ${ARGN})
	else(is_abs_path)
		set(source_dir ${CMAKE_CURRENT_SOURCE_DIR})
		set(files ${file} ${ARGN})
	endif(is_abs_path)
	
	foreach(file_real ${files})
		rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/${file_real}"
			${source_dir}/${file_real})
	endforeach(file_real ${files})
endfunction(rule_copy_source file)

# to_abs_path(output_var path [...])
#
# Convert relative path of file to absolute path:
# use path in source tree, if file already exist there.
# otherwise use path in binary tree.
# If initial path already absolute, return it.
function(to_abs_path output_var)
    set(result)
    foreach(path ${ARGN})
        string(REGEX MATCH "^/" _is_abs_path ${path})
        if(_is_abs_path)
            list(APPEND result ${path})
        else(_is_abs_path)
            file(GLOB to_abs_path_file 
                "${CMAKE_CURRENT_SOURCE_DIR}/${path}"
            )
            if(NOT to_abs_path_file)
                set (to_abs_path_file "${CMAKE_CURRENT_BINARY_DIR}/${path}")
            endif(NOT to_abs_path_file)
            list(APPEND result ${to_abs_path_file})
        endif(_is_abs_path)
    endforeach(path ${ARGN})
    set("${output_var}" ${result} PARENT_SCOPE)
endfunction(to_abs_path output_var path)

#is_path_inside_dir(output_var dir path)
#
# Set output_var to true if path is absolute path inside given directory.
# (!) path should be absolute.
macro(is_path_inside_dir output_var dir path)
    file(RELATIVE_PATH _rel_path ${dir} ${path})
    string(REGEX MATCH "^\\.\\." _is_not_inside_dir ${_rel_path})
    if(_is_not_inside_dir)
        set(${output_var} "FALSE")
    else(_is_not_inside_dir)
        set(${output_var} "TRUE")
    endif(_is_not_inside_dir)
endmacro(is_path_inside_dir output_var dir path)