# Needed for all CMakeLists files, otherwise cmake will warn
cmake_minimum_required (VERSION 2.6)

if (NOT KERNELDIR)
	message(FATAL_ERROR "KERNELDIR should be defined")
endif (NOT KERNELDIR)

project(test_module)
# Create rule for produce file from another file via copiing
macro(copy_source_file FILENAME SOURCE)
add_custom_command(OUTPUT "${FILENAME}"
					COMMAND cp -p ${SOURCE} ${FILENAME}
					DEPENDS "${SOURCE}")
endmacro(copy_source_file FILENAME SOURCE)

# Kbuild system cannot work with out-of-source build,
# so copy all source files into build dir.
copy_source_file("${CMAKE_CURRENT_BINARY_DIR}/module.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/module.c")

copy_source_file("${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	"${CMAKE_CURRENT_SOURCE_DIR}/Kbuild")

# 'make all' should produce test_compile.ko file
add_custom_target(test_module ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test_compile.ko)

set(additional_make_flags)
if(ARCH)
	list(APPEND additional_make_flags "ARCH=${ARCH}")
endif(ARCH)
if(CROSS_COMPILE)
	list(APPEND additional_make_flags "CROSS_COMPILE=${CROSS_COMPILE}")
endif(CROSS_COMPILE)

# Rule to produce try_compile.ko from try_compile.c
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/test_compile.ko"
	COMMAND $(MAKE) ${additional_make_flags} -C ${KERNELDIR} M=${CMAKE_CURRENT_BINARY_DIR} modules
					DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/module.c"
						"${CMAKE_CURRENT_BINARY_DIR}/Kbuild")

