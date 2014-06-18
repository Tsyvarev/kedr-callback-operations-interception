# - Support for building kernel modules

# This module defines several variables which may be used when build
# kernel modules.
#
# The following variables are set here:
#  Kbuild_VERSION_STRING - kernel version.
# Equal to CMAKE_SYSTEM_VERSION.
#  Kbuild_ARCH - architecture.
# May be redefined by the user via 'ARCH'.
#  Kbuild_BUILD_DIR - directory, where the modules are built.
# May be redefined by the user via KBUILD_DIR.
#  Kbuild_VERSION_{MAJOR|MINOR|TWEAK} - kernel version components
#  Kbuild_VERSION_STRING_CLASSIC - classic representation of version,
# where parts are delimited by dots.

# User can control KBUILD_VERSION_STRING via CMAKE_SYSTEM_VERSION variable.
# This will be interpreted by cmake as crosscompile.
set(Kbuild_VERSION_STRING ${CMAKE_SYSTEM_VERSION})

# Form classic version view.
string(REGEX MATCH "([0-9]+)\\.([0-9]+)[.-]([0-9]+)"
    _kernel_version_match
    "${Kbuild_VERSION_STRING}"
)

if(NOT _kernel_version_match)
    message(FATAL_ERROR "Kernel version has unexpected format: ${Kbuild_VERSION_STRING}")
endif(NOT _kernel_version_match)

set(Kbuild_VERSION_MAJOR ${CMAKE_MATCH_1})
set(Kbuild_VERSION_MINOR ${CMAKE_MATCH_2})
set(Kbuild_VERSION_TWEAK ${CMAKE_MATCH_3})
# Version string for compare
set(Kbuild_VERSION_STRING_CLASSIC "${Kbuild_VERSION_MAJOR}.${Kbuild_VERSION_MINOR}.${Kbuild_VERSION_TWEAK}")

# Cache KBUILD_DIR variable only if it is set by the user.
# But set Kbuild_BUILD_DIR variable in any case.
if(KBUILD_DIR)
	set(KBUILD_DIR "${KBUILD_DIR}"
		CACHE PATH "Directory where modules are built(defined by the user)."
	)
	set(Kbuild_BUILD_DIR "${KBUILD_DIR}")
else(KBUILD_DIR)
	set(Kbuild_BUILD_DIR "/lib/modules/${Kbuild_VERSION_STRING}/build")
endif(KBUILD_DIR)

# Cache ARCH variable only if it is set by the user.
# But set Kbuild_ARCH variable in any case.
if(ARCH)
	set(ARCH "${ARCH}"
		CACHE STRING "Architecture for which modules should be built(defined by the user)."
	)
	set(Kbuild_ARCH "${ARCH}")
else(ARCH)
	# Autodetect architecture.
	execute_process(COMMAND uname -m
		RESULT_VARIABLE uname_m_result
		OUTPUT_VARIABLE KBuild_ARCH
	)
	if(NOT uname_m_result EQUAL 0)
		message("'uname -r' failed:")
		message("${KBuild_ARCH}")
		message(FATAL_ERROR "Failed to determine system architecture.")
	endif(NOT uname_m_result EQUAL 0)
	
	# Replace standard patterns.
	# Taken from ./Makefile of the kernel build.
	string(REGEX REPLACE "i.86" "x86" KBuild_ARCH "${KBuild_ARCH}")
	string(REGEX REPLACE "x86_64" "x86" KBuild_ARCH "${KBuild_ARCH}")
	string(REGEX REPLACE "sun4u" "sparc64" KBuild_ARCH "${KBuild_ARCH}")
	string(REGEX REPLACE "arm.*" "arm" KBuild_ARCH "${KBuild_ARCH}")
	string(REGEX REPLACE "sa110" "arm" KBuild_ARCH "${KBuild_ARCH}")
	string(REGEX REPLACE "s390x" "s390" KBuild_ARCH "${KBuild_ARCH}")
	string(REGEX REPLACE "parisc64" "parisc" KBuild_ARCH "${KBuild_ARCH}")
	string(REGEX REPLACE "ppc.*" "powerpc" KBuild_ARCH "${KBuild_ARCH}")
	string(REGEX REPLACE "mips.*" "mips" KBuild_ARCH "${KBuild_ARCH}")
	string(REGEX REPLACE "sh[234].*" "sh" KBuild_ARCH "${KBuild_ARCH}")
	string(REGEX REPLACE "aarch64.*" "arm64" KBuild_ARCH "${KBuild_ARCH}")
endif(ARCH)


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Kbuild
    REQUIRED_VARS Kbuild_BUILD_DIR
    VERSION_VAR KBuild_VERSION_STRING_CLASSIC
)
