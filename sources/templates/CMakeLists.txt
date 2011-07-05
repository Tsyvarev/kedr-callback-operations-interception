# Install template files

set(KEDR_COI_TEMPLATES_PATH "${KEDR_COI_INSTALL_PREFIX_TEMPLATES}")

set(templates
    kedr_coi_interceptor.c
    kedr_coi_interceptor.h
    kedr_coi_interceptor_foreign.c
    kedr_coi_interceptor_foreign.h
)

install(DIRECTORY ${templates} 
        DESTINATION "${KEDR_COI_TEMPLATES_PATH}"
)
