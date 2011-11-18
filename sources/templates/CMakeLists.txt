# Install template files

set(KEDR_COI_TEMPLATES_PATH "${KEDR_COI_INSTALL_PREFIX_TEMPLATES}")

set(templates
    kedr_coi_interceptor.c
    kedr_coi_interceptor.h
    kedr_coi_factory_interceptor.c
    kedr_coi_factory_interceptor.h
    kedr_coi_creation_interceptor.c
    kedr_coi_creation_interceptor.h
)

install(DIRECTORY ${templates} 
        DESTINATION "${KEDR_COI_TEMPLATES_PATH}"
)
