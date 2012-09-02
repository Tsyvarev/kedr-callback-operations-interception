set(kmodule_name "kedr_coi")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

kbuild_add_module(${kmodule_name}
	"kedr_coi_instrumentors_impl.c"
    "kedr_coi_instrumentors.c"
    "kedr_coi_interceptors.c"
    "payloads.c"
	"kedr_coi_module.c"
    

    "kedr_coi_hash_table.c"

	"kedr_coi_instrumentor_internal.h"
    "payloads.h"
    "kedr_coi_hash_table.h"
    )

kedr_coi_install_kmodule(${kmodule_name})
kedr_coi_install_symvers(${kmodule_name})

# For KEDR COI users
set(KEDR_COI_CORE "${KEDR_COI_INSTALL_PREFIX_KMODULE}/${kmodule_name}.ko"
	CACHE INTERNAL "Path to KEDR COI core module"
)

# The tests
kedr_coi_test_add_subdirectory (tests)
