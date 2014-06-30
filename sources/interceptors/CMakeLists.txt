# 
# add_interceptor(name [FACTORY] data_files ... )
#
# Create interceptor of given name from given data_file(s).
# If FACTORY keyword is given, 'factory_' variants of templates will be used.
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
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${name}.c"
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
    
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${name}.h"
	DESTINATION "${KEDR_COI_INSTALL_PREFIX_INCLUDE_KERNEL}/interceptors"
	COMPONENT "devel-kernel"
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${name}.c"
	DESTINATION "${KEDR_COI_INSTALL_PREFIX_INTERCEPTORS}"
	COMPONENT "devel-kernel"
    )
endfunction(add_interceptor name data_file)

# Concrete interceptors
add_subdirectory(file_operations)
add_subdirectory(inode_file_operations)
add_subdirectory(file_system_type)
add_subdirectory(super_operations)
add_subdirectory(inode_operations)
add_subdirectory(dentry_operations)
