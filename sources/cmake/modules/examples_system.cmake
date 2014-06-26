# Common way for create "example", that is set of files which are
# installed "as is".
#
# This set of files is intended to be install into single directory.
#
# For use example, user should copy all files from this directory
# into another location. After that, building or
# whatever-example-provides may be executed.

# Property is set for every target described example.
# Concrete property's value currently has no special meaning.
define_property(TARGET PROPERTY EXAMPLE_TYPE
    BRIEF_DOCS "Whether given target describes example."
    FULL_DOCS "Whether given target describes example."
)

# List of files which example provides.
define_property(TARGET PROPERTY EXAMPLE_FILES
    BRIEF_DOCS "List of files for 'example' target"
    FULL_DOCS "List of files for 'example' target"
)

# List of source files for example.
define_property(TARGET PROPERTY EXAMPLE_SOURCES
    BRIEF_DOCS "List of sources files for 'example' target"
    FULL_DOCS "List of source files for 'example' target"
)

# Whether example target is imported.
#
# Imported example has no EXAMPLE_SOURCES property,
# but may have EXAMPLE_IMPORTED_LOCATION property set (see below).
define_property(TARGET PROPERTY EXAMPLE_IMPORTED
    BRIEF_DOCS "Whether example target is imported."
    FULL_DOCS "Whether example target is imported."
)

# Directory where imported example is located.
define_property(TARGET PROPERTY EXAMPLE_IMPORTED_LOCATION
    BRIEF_DOCS "Directory where imported example is located."
    FULL_DOCS "Directory where imported example is located."
)

#  add_example(name files ...)

# Add example as a set of files.
# Currently only filenames are supported in @files list.
# Each file is taken from source directory, if exists, or from binary directory.
# Original file for 'makefile' should be named 'example_makefile'.

function(add_example name file)
    set(files ${file} ${ARGN})
    # List of full names of source files
    set(source_files_full)
    foreach(f ${files})
        set(source_real ${f})
        if(source_real STREQUAL "makefile")
            set(source_real "example_makefile")
        endif(source_real STREQUAL "makefile")
        to_abs_path(source_file_full ${source_real})
        list(APPEND source_files_full ${source_file_full})
    endforeach(f ${files})
    add_custom_target(${name} ALL
        DEPENDS ${source_files_full})
    set_property(TARGET ${name} PROPERTY EXAMPLE_TYPE "example")
    set_property(TARGET ${name} PROPERTY EXAMPLE_IMPORTED "FALSE")
    set_property(TARGET ${name} PROPERTY EXAMPLE_FILES ${files})
    set_property(TARGET ${name} PROPERTY EXAMPLE_SOURCES ${source_files_full})
endfunction(add_example name files)

# TODO: Way for set properties for example files(e.g., executable).

# install_example(TARGETS <name> [EXPORT <export_name>]
#    DESTINATION <dir>
#    [CONFIGURATIONS ... ]
#    [COMPONENT component]
# )
#
# Install example.
# All files, belonged to the example, are installed into given directory.
#
# <name> should be same as passed to add_example().
# IIf EXPORT option is given, <export_name> will be imported example target,
# binded with install directory as its EXAMPLE_IMPORTED_LOCATION property.
function(install_example first_arg)
    if(NOT first_arg STREQUAL "TARGETS")
        message(FATAL_ERROR "Only TARGETS mode is currently supported")
    endif(NOT first_arg STREQUAL "TARGETS")
    cmake_parse_arguments(install_example "" "EXPORT;DESTINATION;COMPONENT" "CONFIGURATIONS" ${ARGN})
    
    if(NOT install_example_DESTINATION)
        message(FATAL_ERROR "DESTINATION option is missed, but required.")
    endif(NOT install_example_DESTINATION)

    if(install_example_EXPORT)
        set(export_files)
    endif(install_example_EXPORT)
    
    set(install_options)
    if(install_example_COMPONENT)
        list(APPEND install_options COMPONENT ${install_example_COMPONENT})
    endif(install_example_COMPONENT)
    
    if(install_example_CONFIGURATIONS)
        list(APPEND install_options CONFIGURATIONS ${install_example_CONFIGURATIONS})
    endif(install_example_CONFIGURATIONS)
    
    foreach(target ${install_example_UNPARSED_ARGUMENTS})
        get_property(example_sources TARGET ${target} PROPERTY EXAMPLE_SOURCES)
        get_property(example_files TARGET ${target} PROPERTY EXAMPLE_FILES)
        list(LENGTH example_files n_files)
        set(index 0)
        while(index LESS ${n_files})
            list(GET example_sources ${index} example_source)
            list(GET example_files ${index} example_file)
            install(FILES ${example_source}
                DESTINATION ${install_example_DESTINATION}
                ${install_options}
                RENAME ${example_file}
            )
            math(EXPR index "${index} + 1")
            if(install_example_EXPORT)
                list(APPEND export_files ${example_file})
            endif(install_example_EXPORT)
        endwhile(index LESS ${n_files})
    endforeach(target ${install_example_UNPARSED_ARGUMENTS})

    if(install_example_EXPORT)
        add_custom_target(${install_example_EXPORT})

        set_property(TARGET ${install_example_EXPORT} PROPERTY EXAMPLE_TYPE "example")
        set_property(TARGET ${install_example_EXPORT} PROPERTY EXAMPLE_IMPORTED "TRUE")
        set_property(TARGET ${install_example_EXPORT} PROPERTY EXAMPLE_FILES ${export_files})
        set_property(TARGET ${install_example_EXPORT} PROPERTY EXAMPLE_IMPORTED_LOCATION ${install_example_DESTINATION})
    endif(install_example_EXPORT)
endfunction(install_example first_arg)
