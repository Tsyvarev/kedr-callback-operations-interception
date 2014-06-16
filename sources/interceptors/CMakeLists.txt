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

# Concrete interceptors
add_subdirectory(file_operations)
add_subdirectory(inode_file_operations)
add_subdirectory(file_system_type)
add_subdirectory(super_operations)
add_subdirectory(inode_operations)
add_subdirectory(dentry_operations)
