# Directory where example will be installed during testing.
set(example_dir "example")

itesting_path(this_install_dir)

get_property(example_files TARGET read_counter_imported PROPERTY EXAMPLE_FILES)
string(REPLACE ";" " " shell_example_files "${example_files}")

get_property(example_location TARGET read_counter_imported PROPERTY EXAMPLE_IMPORTED_LOCATION)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    DESTINATION "${this_install_dir}"
)

kedr_coi_test_add_script("example.read_counter" "test.sh")