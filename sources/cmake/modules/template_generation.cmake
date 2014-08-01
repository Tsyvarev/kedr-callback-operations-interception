# Location of this CMake module
set(template_generation_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules")
# File with format of deps file, suitable for include()
set(deps_format_file "${template_generation_module_dir}/template_generation_files/deps_format.tpl")

#  jy_generate (<filename> <template_dir> <template_name> <datafiles> ...)
#
# Generate file using jinja2-yaml tool.
#
# <filename> - name of file to be generated.
#  It will be created in the CMAKE_CURRENT_BINARY_DIR.
# <templates_dir> - directory with templates used for generation.
# <template_name> - name of the template to be instantiated.
# <datafiles> - file(s) with template data
#
# Any non-absolute path in <datafiles> is interpreted relative to
# CMAKE_CURRENT_SOURCE_DIR if such file exists, or relative to
# CMAKE_CURRENT_BINARY_DIR (see to_abs_path() function).
function(jy_generate filename templates_dir template_name datafile)
    to_abs_path(datafiles_abs ${datafile} ${ARGN})
    set(output_file "${CMAKE_CURRENT_BINARY_DIR}/${filename}")

    # File contained dependencies definitions of generation process.
    set(deps_file "${CMAKE_CURRENT_BINARY_DIR}/.${filename}.deps")

    set(deps_list)
    # 'deps_file' set 'deps_list' variable to list of real dependencies.
    include("${deps_file}" OPTIONAL)

    # Files generated also depends on yaml-datafiles list.
    # So, if any file will be removed from that list,
    # generation process automatically repeats.
    set(list_filename "${CMAKE_CURRENT_BINARY_DIR}/.${filename}.list")
    string(REPLACE ";" "\n" list_file_content "${data_files_abs}")
    file_update(${list_filename} "${list_file_content}")

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
        COMMENT "jinja2-yaml template generation of ${filename}"
    )
endfunction(jy_generate filename template_dir template_name)