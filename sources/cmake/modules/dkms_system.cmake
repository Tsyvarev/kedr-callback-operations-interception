# Support for building and installing cmake project using DKMS.
#
# Usual workflow is follow:
# 1. Create dkms target using dkms_add() with name and version for
#     DKMS package created.
# 2. Install() sources of cmake project into some directory.
# 3. For every kernel module intended to be installed using DKMS
#     mechanism call dkms_add_module()
# 3. Set DKMS_CMAKE_FLAGS property for dkms target to ones, which should
#     be passed for configure project installed at step 2.
#     With these flags only kernel-dependent files should be built,
#     and for kernel module(s) intended to be installed using DKMS
#     mehanism (see step 3) install() directive should be omitted.
# 4. Install dkms.conf and auxiliary DKMS files using dkms_install(),
#     passing install directory with sources (see step 2) to it.
# 
# Now, after installing current cmake project, you can build and install
# cmake project  from step 2 using standard DKMS actions 'dkms build'
# and 'dkms install'.
#
# NOTE: Using dkms build, you can build project only for kernel which is
# currently loaded. Building for other kernel requires cross compiling,
# which is not currently implemented.

#  dkms_add(<target_name> <package_name> <package_version>)
#
# Add target which describe DKMS package.
#
# Target is used only for properties contained package parameters,
# so it shouldn't be built.
function(dkms_add target_name package_name package_version)
    add_custom_target(${target_name})
    set_property(TARGET ${target_name} PROPERTY DKMS_PACKAGE_NAME ${package_name})
    set_property(TARGET ${target_name} PROPERTY DKMS_PACKAGE_VERSION ${package_version})
endfunction(dkms_add)


#  TARGET PROPERTY DKMS_CMAKE_FLAGS
#
# This property should be set to cmake flags list, which should be
# passed at configuration stage when DKMS package will be built.
#
# Note, that standard dkms definitions are passed automatically:
#  -DCMAKE_SYSTEM_VERSION=$kernelversion [not implemented]
#  -DKBUILD_DIR=$kernel_source_dir
define_property(TARGET PROPERTY DKMS_CMAKE_FLAGS
    BRIEF_DOCS "CMake flags used for dkms build action."
    FULL_DOCS "CMake flags used for dkms build action."
)

#  dkms_add_module(dkms_target <module_name> <build_location> <install_location> [install_name])
#
# Add kernel module for being built using DKMS mechanism.
#
# <module_name> should be name of the module, located in <build_location>
# relative to build tree(this tree will be created at 'dkms build' action).
#
# <install_location> give location relative to common directory
# with kernel modules. Note, that on some distros install location for the
# module is determined automatically.
#
# Iff module is needed to be renamed after install, <install_name> should be given.
function(dkms_add_module dkms_target module_name build_location install_location)
    set(install_name ${module_name})
    foreach(arg ${ARGN})
        set(install_name ${arg})
    endforeach(arg ${ARGN})
    
    if(IS_ABSOLUTE ${build_location})
        message(FATAL_ERROR "<build_location> should be relative path")
    endif(IS_ABSOLUTE ${build_location})
    
    if(IS_ABSOLUTE ${install_location})
        message(FATAL_ERROR "<install_location> should be relative path")
    endif(IS_ABSOLUTE ${install_location})


    set_property(TARGET ${dkms_target} APPEND PROPERTY DKMS_BUILT_MODULE_NAME ${module_name})
    set_property(TARGET ${dkms_target} APPEND PROPERTY DKMS_BUILT_MODULE_LOCATION ${build_location})
    set_property(TARGET ${dkms_target} APPEND PROPERTY DKMS_DEST_MODULE_LOCATION ${install_location})
    set_property(TARGET ${dkms_target} APPEND PROPERTY DKMS_DEST_MODULE_NAME ${install_name})
endfunction(dkms_add_module)

#  dkms_install(dkms_target
#   [DESTINATION <dir>]
#   [SOURCES <sources_dir>]
#   [COMPONENT <component>]
#   [MODULES_ONLY])
#
# Install all needed DKMS files.
#
# Default installation directory(if DESTINATION option is not specified)
# is /usr/src/<package_name>-<package_version>.
#
# In this case package is automatically detected by dkms when
# specified with <pachage_name>/<package_version> pair
# (see dkms documentation).
#
# Otherwise, if DESTINATION option is given and <dir> value is differ
# from default directory, 'dkms add <dir>' action should be issued for
# make dkms aware of the package. After this action all files from
# <dir> directory will be copied into /usr/src/<package_name>-<package_version>.
#
# By default, source files for the cmake project are expected to be
# installed into same directory as dkms files, that is
# /usr/src/<package_name>-<package_version>
# or
# <dir> given by DESTINATION option.
# One can redefine source files location by SOURCES option. This is
# recommended, since this prevent files to be copied by dkms at
# 'dkms add' and 'dkms build' actions.
# (See dkms documentation about directories which are copied automatically.)
# 
# Iff MODULES_ONLY option is given, only modules added with dkms_add_module()
# will be installed at 'dkms install' action, otherwise all files,
# intended for installation in cmake scripts, will be installed too.
#
# Note, that additional files will be uninstalled only at 'dkms remove'
# action, not at 'dkms uninstall' one.
# (As 'dkms uninstall' action does not support hooks.)
function(dkms_install dkms_target)
    cmake_parse_arguments(dkms_install "MODULES_ONLY" "DESTINATION;SOURCES;COMPONENT" "" ${ARGN})
    
    get_property(package_name TARGET ${dkms_target} PROPERTY DKMS_PACKAGE_NAME)
    get_property(package_version TARGET ${dkms_target} PROPERTY DKMS_PACKAGE_VERSION)
    
    if(NOT dkms_install_DESTINATION)
        set(dkms_install_DESTINATION "/usr/src/${package_name}-${package_version}")
    endif(NOT dkms_install_DESTINATION)
    
    if(NOT dkms_install_SOURCES)
        set(dkms_install_SOURCES "${dkms_install_DESTINATION}")
    endif(NOT dkms_install_SOURCES)
    
    set(COMPONENT_option)
    if(dkms_install_COMPONENT)
        set(COMPONENT_option "COMPONENT" "${dkms_install_COMPONENT}")
    endif(dkms_install_COMPONENT)
    
    set(modules)
    get_property(module_name TARGET ${dkms_target} PROPERTY DKMS_BUILT_MODULE_NAME)
    get_property(build_location TARGET ${dkms_target} PROPERTY DKMS_BUILT_MODULE_LOCATION)
    get_property(install_location TARGET ${dkms_target} PROPERTY DKMS_DEST_MODULE_LOCATION)
    get_property(install_name TARGET ${dkms_target} PROPERTY DKMS_DEST_MODULE_NAME)
    
    list(LENGTH module_name n_modules)
    set(i "0")
    while(i LESS n_modules)
        list(GET module_name ${i} module_name_i)
        list(GET build_location ${i} build_location_i)
        list(GET install_location ${i} install_location_i)
        list(GET install_name ${i} install_name_i)
        
        set(install_name_i_def)
        if(NOT module_name_i STREQUAL install_name_i)
            set(install_name_i_def "DEST_MODULE_NAME[${i}]=${install_name_i}\n")
        endif(NOT module_name_i STREQUAL install_name_i)
        set(module_i
"BUILT_MODULE_NAME[${i}]=${module_name_i}
BUILT_MODULE_LOCATION[${i}]=build/${build_location_i}
DEST_MODULE_LOCATION[${i}]=/${install_location_i}
${install_name_i_def}"
)
        set(modules "${modules}${module_i}\n")
        
        math(EXPR i "${i}+1")
    endwhile(i LESS n_modules)
    
    if(NOT dkms_install_MODULES_ONLY)
        # For install additional files we need to perform 'make install'
        # at 'dkms build' action, which install files into temporary location.
        set(build_target "install")
        # Also we need POST_INSTALL and POST_REMOVE hooks for additional
        # files' real installation and removal on corresponded dkms actions.
        set(hooks
"POST_INSTALL=\"dkms_scripts/install_script.sh install \${kernelver}\"
POST_REMOVE=\"dkms_scripts/install_script.sh remove \${kernelver}\""
)
    else(NOT dkms_install_MODULES_ONLY)
        set(build_target "all")
        set(hooks)
    endif(NOT dkms_install_MODULES_ONLY)

    # All configured files are located in ${dkms_target} subdir in the build tree.
    get_property(dkms_cmake_flags_list TARGET ${dkms_target} PROPERTY DKMS_CMAKE_FLAGS)
    string(REPLACE ";" " " dkms_cmake_flags "${dkms_cmake_flags_list}")
    
    configure_file("${dkms_this_module_dir}/dkms_system_files/dkms.conf.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${dkms_target}/dkms.conf"
        @ONLY
    )
    
    configure_file("${dkms_this_module_dir}/dkms_system_files/makefile_dkms.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${dkms_target}/makefile_dkms"
        @ONLY
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${dkms_target}/dkms.conf"
        "${CMAKE_CURRENT_BINARY_DIR}/${dkms_target}/makefile_dkms"
        DESTINATION ${dkms_install_DESTINATION}
        ${COMPONENT_option}
    )

    if(NOT dkms_install_MODULES_ONLY)
        configure_file("${dkms_this_module_dir}/dkms_system_files/install_script.sh.in"
            "${CMAKE_CURRENT_BINARY_DIR}/${dkms_target}/install_script.sh"
            @ONLY
        )

        install(PROGRAMS
            "${CMAKE_CURRENT_BINARY_DIR}/${dkms_target}/install_script.sh"
            DESTINATION ${dkms_install_DESTINATION}/dkms_scripts
            ${COMPONENT_option}
        )
    endif(NOT dkms_install_MODULES_ONLY)
endfunction(dkms_install)


#######################################################################
# Auxiliary definitions and functions.
get_filename_component(dkms_this_module_dir ${CMAKE_CURRENT_LIST_FILE} PATH)