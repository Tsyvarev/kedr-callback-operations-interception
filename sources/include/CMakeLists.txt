# List of include files that do not need configuring and
# should be copied to the binary tree as is.
set(KEDR_COI_INCLUDE_FILES_SRC
	operations_interception.h
)


# List of include files to be installed.
set(KEDR_COI_INCLUDE_FILES_INSTALL
	operations_interception.h
)

# Copy headers from src-tree into binary tree
foreach(include_file ${KEDR_COI_INCLUDE_FILES_SRC})
    set(include_file_src "${CMAKE_CURRENT_SOURCE_DIR}/kedr-coi/${include_file}")
	configure_file("${include_file_src}" "${CMAKE_CURRENT_BINARY_DIR}/kedr-coi/${include_file}" COPYONLY)
endforeach(include_file ${KEDR_COI_INCLUDE_FILES_SRC})

if (NOT CMAKE_CROSSCOMPILING)
# Install rules for include files (files will be installed from binary tree!)
	foreach(include_file ${KEDR_COI_INCLUDE_FILES_INSTALL})
		get_filename_component(dest_dir "${KEDR_COI_INSTALL_PREFIX_INCLUDE}/${include_file}" PATH)
	    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kedr-coi/${include_file}"
	            DESTINATION "${dest_dir}")
	endforeach(include_file ${KEDR_COI_INCLUDE_FILES_INSTALL})

# Copy header files for examples.
#
# Because examples can use only installed header files, these files
# are copied into directory different from one for files used by
# components of the KEDR internally. These strategy allows to detect 
# missing install headers at the testing stage.
	foreach(include_file ${KEDR_COI_INCLUDE_FILES_INSTALL})
	    set(include_file_src "${CMAKE_CURRENT_BINARY_DIR}/kedr-coi/${include_file}")
		get_filename_component(dest_dir "${KEDR_COI_TEST_PREFIX_INCLUDE}/${include_file}" PATH)
		file(MAKE_DIRECTORY "${dest_dir}")
		execute_process(COMMAND cp -p "${include_file_src}" "${dest_dir}"
			RESULT_VARIABLE copy_result)
		if(copy_result)
			message(FATAL_ERROR "Failed to copy header file ${include_file} for examples.")
		endif(copy_result)
	endforeach(include_file ${KEDR_COI_INCLUDE_FILES_SRC})
endif (NOT CMAKE_CROSSCOMPILING)
