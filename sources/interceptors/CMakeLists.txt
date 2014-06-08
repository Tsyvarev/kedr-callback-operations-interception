# Directory where generated source files will be placed.
# Currently this directory is coincide with directory of interceptors
# used for tests.
set(source_files_dir "${KEDR_COI_TEST_PREFIX_INTERCEPTORS}")

execute_process(
	COMMAND mkdir -p "${source_files_dir}"
)

# Directory where generated header files will be placed.
set(header_files_dir "${KEDR_COI_TEST_PREFIX_INCLUDE_KERNEL}/interceptors")

execute_process(
	COMMAND mkdir -p "${header_files_dir}"
)

set(KEDR_COI_TEMPLATES_DIR "${CMAKE_SOURCE_DIR}/templates")

# 
# add_interceptor(name [FACTORY] data_files ... )
#
# Create interceptor of given name from given data_file(s).
# If FACTORY keyword is given, 'factory_' variants of templates will be used.
#
# Header file will be stored in ${header_files_dir}, source file - 
# in ${source_files_dir}. Also both files will be installed into
# corresponded destinations.
#
# NOTE: Function is intended to be called from CMakeLists.txt in 
# subdirectories, not from the current CMakeLists.txt.

function(add_interceptor name data_file)
    set(data_files_abs)
    set(template_prefix "")
    foreach(f ${data_file} ${ARGN})
	if(f STREQUAL "FACTORY")
	    set(template_prefix "factory_")
	else(f STREQUAL "FACTORY")
	    to_abs_path(data_file_abs ${f})
	    list(APPEND data_files_abs ${data_file_abs})
	endif(f STREQUAL "FACTORY")
    endforeach(f ${data_file} ${ARGN})

    add_custom_target(${name} ALL
	DEPENDS "${source_files_dir}/${name}.c"
		"${header_files_dir}/${name}.h"
    )
    
    rule_copy_file("${source_files_dir}/${name}.c"
	"${CMAKE_CURRENT_BINARY_DIR}/${name}.c"
    )
    rule_copy_file("${header_files_dir}/${name}.h"
	"${CMAKE_CURRENT_BINARY_DIR}/${name}.h"
    )
    
    jy_generate("${name}.c" "${KEDR_COI_TEMPLATES_DIR}/kedr_coi_interceptor"
	"${template_prefix}interceptor_c"
	${data_files_abs}
    )
    
    jy_generate("${name}.h" "${KEDR_COI_TEMPLATES_DIR}/kedr_coi_interceptor"
	"${template_prefix}interceptor_h"
	${data_files_abs}
    )
    
    install(FILES "${header_files_dir}/${name}.h"
	DESTINATION "${KEDR_COI_INSTALL_PREFIX_INCLUDE_KERNEL}/interceptors")
    install(FILES "${source_files_dir}/${name}.c"
	DESTINATION "${KEDR_COI_INSTALL_PREFIX_INTERCEPTORS}")
endfunction(add_interceptor name data_file)

# create_predefined_interceptor_header(header_filename data_file [FACTORY])
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
    set(template_class "kedr_coi_interceptor")
    foreach(arg ${ARGN})
	if(arg STREQUAL "FACTORY")
	    set(template_class "kedr_coi_factory_interceptor")
	endif(arg STREQUAL "FACTORY")
    endforeach(arg ${ARGN})

    jy_generate_file(${header_filename} ${data_file}
	"${KEDR_COI_TEMPLATES_DIR}/${template_class}.h"
    )
    # Install this header file
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${header_filename}"
        DESTINATION "${KEDR_COI_INSTALL_PREFIX_INCLUDE}/interceptors")
    # Make this header file available for testing
    execute_process(COMMAND cp -p -t "${KEDR_COI_TEST_PREFIX_INCLUDE}/interceptors"
        "${CMAKE_CURRENT_BINARY_DIR}/${header_filename}"
        )
endfunction(create_predefined_interceptor_header header data_file)


# create_predefined_interceptor_source(source_filename data_file [FACTORY])
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
    set(template_class "kedr_coi_interceptor")
    foreach(arg ${ARGN})
	if(arg STREQUAL "FACTORY")
	    set(template_class "kedr_coi_factory_interceptor")
	endif(arg STREQUAL "FACTORY")
    endforeach(arg ${ARGN})
    # Generate source file
    jy_generate_file(${source_filename} ${data_file}
	"${KEDR_COI_TEMPLATES_DIR}/${template_class}.c")
    # Install generated file
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${source_filename}"
        DESTINATION "${KEDR_COI_INSTALL_PREFIX_INTERCEPTORS}")
    # Create file's copy in the @source_files_dir.
    execute_process(COMMAND cp -p -t "${source_files_dir}"
        "${CMAKE_CURRENT_BINARY_DIR}/${source_filename}")

endfunction(create_predefined_interceptor_source source_filename data_file)


# Concrete interceptors
add_subdirectory(file_operations)
add_subdirectory(inode_file_operations)
add_subdirectory(file_system_type)
add_subdirectory(super_operations)
add_subdirectory(inode_operations)
add_subdirectory(dentry_operations)
