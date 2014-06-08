# Install template files

set(KEDR_COI_TEMPLATES_PATH "${KEDR_COI_INSTALL_PREFIX_TEMPLATES}")

set(templates
    "kedr_coi_interceptor"
    "kedr_trampolines.c"
)

install(DIRECTORY ${templates} 
        DESTINATION "${KEDR_COI_TEMPLATES_PATH}"
)
