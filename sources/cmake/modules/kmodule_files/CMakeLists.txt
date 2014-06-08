# Needed for all CMakeLists files, otherwise cmake will warn
cmake_minimum_required (VERSION 2.6)

project(hello)
# Create rule for produce file from another file via copiing
macro(copy_source_file FILENAME SOURCE)
add_custom_command(OUTPUT "${FILENAME}"
					COMMAND cp -p ${SOURCE} ${FILENAME}
					MAIN_DEPENDENCY "${SOURCE}")
endmacro(copy_source_file FILENAME SOURCE)

if(NOT DEFINED SRC_FILE)
	message(FATAL_ERROR "'SRC_FILE' should be defined, but it is not.")
endif(NOT DEFINED SRC_FILE)

configure_file(Kbuild.in Kbuild)
# 'make all' should produce try_compile.ko file
add_custom_target(kmodule_compile ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/try_compile.ko)
# Rule to produce try_compile.ko from try_compile.c

set(additional_make_flags)
if(ARCH)
	list(APPEND additional_make_flags "ARCH=${ARCH}")
endif(ARCH)
if(CROSS_COMPILE)
	list(APPEND additional_make_flags "CROSS_COMPILE=${CROSS_COMPILE}")
endif(CROSS_COMPILE)


add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/try_compile.ko"
	COMMAND $(MAKE) ${additional_make_flags} -C ${KERNELDIR} M=${CMAKE_BINARY_DIR} modules
					DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/try_compile.c")

# Kbuild system cannot work with out-of-source build,
# so copy source file into build dir and rename it to try_compile.c.
copy_source_file("${CMAKE_CURRENT_BINARY_DIR}/try_compile.c" "${SRC_FILE}")
