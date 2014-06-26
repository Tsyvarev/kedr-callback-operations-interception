itesting_path(this_install_dir)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY)

install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    DESTINATION "${this_install_dir}"
)

kedr_coi_test_add_script("dummy_test" "test.sh")