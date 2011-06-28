set(kmodule_name "kedr_coi")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

kbuild_add_module(${kmodule_name}
	"kedr_coi_module.c"
    "kedr_coi_base.c"
    "kedr_coi_interceptors.c"
    "kedr_coi_instrumentors.c"

    "kedr_coi_hash_table.c"
    "kedr_coi_global_map.c"
    kedr_coi_payloads_container.c
    kedr_coi_payloads_foreign_container.c

	"kedr_coi_base_internal.h"
	"kedr_coi_instrumentor_internal.h"
    "kedr_coi_hash_table_internal.h"
    "kedr_coi_global_map_internal.h"
    "kedr_coi_payloads_internal.h"
    "kedr_coi_payloads_foreign_internal.h"


    )

kedr_coi_install_kmodule(${kmodule_name})
kedr_coi_install_symvers(${kmodule_name})

# The tests
#kedr_test_add_subdirectory (tests)
