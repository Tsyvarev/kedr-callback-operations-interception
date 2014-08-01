# Generator-specific declarations for CPack.
#
# This file is include via CPACK_PROJECT_CONFIG_FILE variable.
#
# CPACK_GENERATOR is single valued-here.
# Variables set there are described at http://www.cmake.org/Wiki/CMake:CPackPackageGenerators.

# RPM
set(CPACK_RPM_PACKAGE_LICENSE "GPL2")

# Explicitely enable component-based behaviour for RPM.
#
# See http://www.cmake.org/Wiki/CMake:Component_Install_With_CPack#CPack_Generator_specific_behavior
set(CPACK_RPM_COMPONENT_INSTALL "1")
