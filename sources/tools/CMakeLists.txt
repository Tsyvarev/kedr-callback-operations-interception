if (NOT CMAKE_CROSSCOMPILING)
# "kedr_gen" for outer use (including usage in examples)
# Also need to set ${KEDR_GEN_TOOL_INSTALLED} as the path to "kedr_gen".
# It can be used in examples.
    if(KEDR_GEN_INSTALLED)
        SET(KEDR_GEN_TOOL_INSTALLED "${KEDR_GEN_INSTALLED}" CACHE INTERNAL "Path to the kedr_gen used for examples")
    else(KEDR_GEN_INSTALLED)
    	set (KEDR_GEN_INSTALL_PREFIX ${KEDR_COI_INSTALL_PREFIX_EXEC_AUX})
    	add_subdirectory(kedr_gen)
        
        SET(KEDR_GEN_TOOL_INSTALLED "${KEDR_COI_INSTALL_PREFIX_EXEC_AUX}/kedr_gen" CACHE INTERNAL "Path to the kedr_gen used for examples")
    endif(KEDR_GEN_INSTALLED)
endif (NOT CMAKE_CROSSCOMPILING)
