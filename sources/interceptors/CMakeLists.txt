# Directory where generated source files will be placed.
# Currently this directory is coincide with directory of interceptors
# used for tests.
set(source_files_dir "${KEDR_COI_TEST_PREFIX_INTERCEPTORS}")

execute_process(
	COMMAND mkdir -p "${source_files_dir}"
)

# Directory where generated header files will be placed.
set(header_files_dir "${CMAKE_CURRENT_BINARY_DIR}/include-files")

execute_process(
	COMMAND mkdir -p "${header_files_dir}"
)

execute_process(
	COMMAND mkdir -p "${KEDR_COI_TEST_PREFIX_INCLUDE}/kedr-coi/interceptors"
)


# create_predefined_interceptor_header(header_filename data_file)
#
# Create header file of interceptor from datafile.
#
# Created header file will be installed, so it may be used by examples.
#
# In current implementation created header file may not be used in
# building of the project(unlike to "operations_interceptor.h" header,
# which is used when compiling KEDR COI core module).
#
# 'header_filename' - is a relative name of the header file, this file
# will be placed under "kedr-coi/interceptors" subdirectory in include tree.
#
# In current implementation 'header_filename' may be only file without directory.
#

function(create_predefined_interceptor_header header_filename data_file)
    # Generate header file
	execute_process(
		COMMAND "${KEDR_GEN_TOOL}" "${KEDR_GEN_COI_TEMPLATES_DIR}/kedr_coi_interceptor.h" "${data_file}"
		RESULT_VARIABLE generate_result
		OUTPUT_FILE "${header_files_dir}/${header_filename}"
		ERROR_VARIABLE generate_error
	)
	if(generate_result)
		message("Error: ${generate_error}")
		message(FATAL_ERROR "Fail to generate interceptor header file \"${header_filename}\".")
	endif(generate_result)
    # Install this header file
    install(FILES "${header_files_dir}/${header_filename}"
        DESTINATION "${KEDR_COI_INSTALL_PREFIX_INCLUDE}/kedr-coi/interceptors")
    # Make this header file available for testing
    execute_process(COMMAND cp -p -t "${KEDR_COI_TEST_PREFIX_INCLUDE}/kedr-coi/interceptors"
        "${header_files_dir}/${header_filename}"
        )
endfunction(create_predefined_interceptor_header header data_file)


# create_predefined_interceptor_source(source_filename data_file)
#
# Create source file of interceptor from datafile.
#
# Created source file will be installed, so it may be used by examples.
#
# 'source_filename' - is a relative name of the source file, this file
# will be placed under in "interceptors" directory.
#
# In current implementation 'source_filename' may be only file without directory.
#


function(create_predefined_interceptor_source source_filename data_file)
	# Generate source file
    execute_process(
		COMMAND "${KEDR_GEN_TOOL}" "${KEDR_GEN_COI_TEMPLATES_DIR}/kedr_coi_interceptor.c" "${data_file}"
		RESULT_VARIABLE generate_result
		OUTPUT_FILE "${source_files_dir}/${source_filename}"
		ERROR_VARIABLE generate_error
	)
	if(generate_result)
		message("Error: ${generate_error}")
		message(FATAL_ERROR "Fail to generate interceptor source file \"${source_filename}\".")
	endif(generate_result)
    # Install generated file
    install(FILES "${source_files_dir}/${source_filename}"
        DESTINATION "${KEDR_COI_INSTALL_PREFIX_INTERCEPTORS}")
    # Generated source file already available for testing.
endfunction(create_predefined_interceptor_source source_filename data_file)


# Concrete interceptors
add_subdirectory(file_operations)