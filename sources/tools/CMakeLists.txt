SET(JY_TOOL "${CMAKE_CURRENT_SOURCE_DIR}/jinja2-yaml/jinja2-yaml.py" CACHE INTERNAL "Path to the jinja2-yaml in build process")
if (NOT CMAKE_CROSSCOMPILING)
# "jinja2-yaml" for outer use (including usage in examples)
# Also need to set ${JY_TOOL_INSTALLED} as the path to "jinja2-yaml".
# It can be used in examples.
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/jinja2-yaml/jinja2-yaml.py"
        DESTINATION ${KEDR_COI_INSTALL_PREFIX_EXEC_AUX}
        PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)
    SET(JY_TOOL_INSTALLED "${KEDR_COI_INSTALL_PREFIX_EXEC_AUX}/jinja2-yaml.py" CACHE INTERNAL "Path to the jinja2-yaml tool used for examples")
endif (NOT CMAKE_CROSSCOMPILING)
