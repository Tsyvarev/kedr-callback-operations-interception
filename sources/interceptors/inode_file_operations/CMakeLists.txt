set(datafile "interceptor.data")

create_predefined_interceptor_header("inode_file_operations_interceptor.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/${datafile}" FACTORY)

create_predefined_interceptor_source("inode_file_operations_interceptor.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/${datafile}" FACTORY)