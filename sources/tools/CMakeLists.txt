SET(JY_TOOL "python" "${CMAKE_CURRENT_SOURCE_DIR}/jinja2-yaml/jinja2-yaml.py" CACHE INTERNAL "jinja2-yaml for build process")

if(USER_PART)
    if (NOT CMAKE_CROSSCOMPILING)
    # "jinja2-yaml" for outer use (including usage in examples).
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/jinja2-yaml/jinja2-yaml.py"
            DESTINATION ${KEDR_COI_INSTALL_PREFIX_EXEC_AUX}
            COMPONENT "devel"
            PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)
    endif (NOT CMAKE_CROSSCOMPILING)
endif(USER_PART)