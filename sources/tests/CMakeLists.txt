include(kedr_coi_testing)

kedr_coi_test_init("${KEDR_COI_INSTALL_PREFIX_VAR}/tests")
# KEDR_COI_INSTALL_PREFIX_VAR already marked for clean at uninstall stage.

if(USER_PART)
    add_subdirectory(dummy)
endif(USER_PART)

add_subdirectory(core)

if(USER_PART)
    add_subdirectory(examples)
endif(USER_PART)

