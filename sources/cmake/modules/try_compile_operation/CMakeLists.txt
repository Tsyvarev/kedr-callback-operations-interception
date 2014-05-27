# Needed for all CMakeLists files, otherwise cmake will warn
cmake_minimum_required (VERSION 2.6)

if (NOT KERNELDIR)
	set(KERNELDIR /lib/modules/${KBUILD_VERSION_STRING}/build)
endif (NOT KERNELDIR)

project(kmodule_try_compile_operation)

if(NOT IS_ABSOLUTE ${HEADER_FILE})
	message(FATAL_ERROR "HEADER_FILE parameter should be absolute path")
endif(NOT IS_ABSOLUTE ${HEADER_FILE})

string(REGEX REPLACE "/[^/]+$" "/" include_dir ${HEADER_FILE})
string(REGEX REPLACE ".*/(.)" "\\1" include_file ${HEADER_FILE})

if(DEFINED OPERATION_TYPE)
	configure_file("try_compile_operation_typed.c.in" "try_compile_operation.c"
		@ONLY)
else(DEFINED OPERATION_TYPE)
	configure_file("try_compile_operation.c.in" "try_compile_operation.c"
		@ONLY)
endif(DEFINED OPERATION_TYPE)

configure_file("Kbuild.in" "Kbuild"
	@ONLY)
# 'make all' should produce try_compile.ko file
add_custom_target(kmodule_compile ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/try_compile_operation.ko)
# Rule to produce try_compile.ko from try_compile.c
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/try_compile_operation.ko"
	COMMAND $(MAKE) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C ${KERNELDIR} M=${CMAKE_CURRENT_BINARY_DIR} modules
					DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/try_compile_operation.c")
