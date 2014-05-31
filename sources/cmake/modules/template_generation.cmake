# Location of this CMake module
set(template_generation_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules")
# File with format of deps file, suitable for include()
set(deps_format_file "${template_generation_module_dir}/template_generation_files/deps_format.tpl")

# Script for update dependencies for template generation process.
#set(update_dependencies_script "${template_generation_module_dir}/template_generation_files/update_deps.sh")

# Generate file using jinja2-yaml tool.
#
# jy_generate (filename template_dir template_name datafile ...)
#
# filename - name of file to be generated. File will be created in the
# CMAKE_CURRENT_BINARY_DIR.
# templates_dir - directory with templates used for generation.
# template_name - name of the template to be instantiated.
# datafile - file(s) with template data
#
# If 'datafile' contains non-absolute path, it is assumed to be relative
# to CMAKE_CURRENT_SOURCE_DIR if file exists, or to CMAKE_CURRENT_BINARY_DIR
# (see to_abs_path() function).
function(jy_generate filename templates_dir template_name datafile)
    to_abs_path(datafiles_abs ${datafile} ${ARGN})
    set(output_file "${CMAKE_CURRENT_BINARY_DIR}/${filename}")

    # File contained dependencies definitions of generation process.
    set(deps_file "${CMAKE_CURRENT_BINARY_DIR}/.${filename}.deps")

    # Usually, templates are static files, and already exist at
    # 'configure' stage(cmake command executing).
    # So perform dependencies generation at that stage.
    #
    # If template files are created at build stage, then dependencies
    # will be regenerated at the first build. In that case second build
    # call(with unchanged templates) will trigger automatic reconfiguration,
    # but build stage itself will not do anything.
    #execute_process(COMMAND sh "${update_dependencies_script}" "${deps_file}" ${template_dir}
    #RESULT_VARIABLE setup_dependency_result
    #)
    #if(setup_dependency_result)
    #message(FATAL_ERROR "Failed to generate dependencies of template generation for ${output_file}")
    #endif(setup_dependency_result)
    
    
    set(deps_list)
    # 'deps_file' set 'deps_list' variable to list of real dependencies.
    include("${deps_file}" OPTIONAL)

    # Files generated also depends on yaml-datafiles list.
    # So, if any file will be removed from that list,
    # generation process automatically repeats.
    set(list_filename "${CMAKE_CURRENT_BINARY_DIR}/.${filename}.list")
    string(REPLACE ";" "\n" list_file_content "${data_files_abs}")
    write_or_update_file(${list_filename} "${list_file_content}")


	# When instantiate template, also update dependencies file
	# if dependencies changed.
	# Because 'deps_file' is used in include() cmake command, its
	# changing will trigger reconfiguration at the next build.

    add_custom_command(OUTPUT "${output_file}"
# Add ${deps_file} for output. It allows to simply remove it via 'make clean'
# when some dependencies was removed without preliminary rebuilding.
        "${deps_file}"
        COMMAND ${JY_TOOL} -o "${output_file}" -t ${template_name}
            -d "${deps_file}" -F "${deps_format_file}"
            ${templates_dir} ${datafiles_abs}
        DEPENDS ${datafiles_abs} ${deps_list} ${list_filename}
    )
    # Add empty rules for generate dependencies files.
    #
    # It is useful when some template files will be removed:
    # without these rules build process will be terminated with error
    # (because of absent dependency file).
    #
    # Note, that output file will be rebuilt in that case, so dependencies
    # will be recalculated.
    #
    #
    # But with that dependecies original templates will be removed on "make clean".
    # foreach(dep ${deps_list})
    #    add_custom_command(OUTPUT ${dep} COMMAND /bin/true)
    # endforeach(dep ${deps_list})
endfunction(jy_generate filename template_dir datafile)

# Same as jy_generate(), but generate file at configuration stage
function(jy_generate_file filename datafile template_dir)
    to_abs_path(datafile_abs ${datafile})
    set(output_file "${CMAKE_CURRENT_BINARY_DIR}/${filename}")

    # File contained dependencies definitions of generation process.
    set(deps_file "${CMAKE_CURRENT_BINARY_DIR}/.${filename}.deps")

    # Usually, templates are static files, and already exist at
    # 'configure' stage(cmake command executing).
    # So perform dependencies generation at that stage.
    #
    # If template files are created at build stage, then dependencies
    # will be regenerated at the first build. In that case second build
    # call(with unchanged templates) will trigger automatic reconfiguration,
    # but build stage itself will not do anything.
    #execute_process(COMMAND sh "${update_dependencies_script}" "${deps_file}" ${template_dir}
    #RESULT_VARIABLE setup_dependency_result
    #)
    #if(setup_dependency_result)
    #message(FATAL_ERROR "Failed to generate dependencies of template generation for ${output_file}")
    #endif(setup_dependency_result)
    
    set(need_generate)
    if(EXISTS "${deps_file}")
        include("${deps_file}")
        # Re-execute template instantiation only when one of deps file
        # is newer than the target.
        foreach(dep ${datafile_abs} ${deps_list})
            if("${dep}" IS_NEWER_THAN "${output_file}")
                set(need_generate 1)
                break()
            endif("${dep}" IS_NEWER_THAN "${output_file}")
        endforeach(dep ${datafile_abs} ${deps_list})
    else(EXISTS "${deps_file}")
        set(need_generate 1)
    endif(EXISTS "${deps_file}")
    if(need_generate)
        execute_process(
            COMMAND ${JY_TOOL} -o "${output_file}"
            -d "${deps_file}" -f "${deps_file_format}"
            ${template_dir} ${datafile_abs}
            RESULT_VARIABLE generate_result
        )
        if(generate_result)
            message(FATAL_ERROR "111")
        endif(generate_result)
    endif(need_generate)
endfunction(jy_generate_file filename datafile template_dir)
