set(kmodule_name "kedr_coi")

# For KEDR COI users
set(KEDR_COI_CORE "${KEDR_COI_INSTALL_PREFIX_KMODULE}/${kmodule_name}.ko"
    CACHE INTERNAL "Path to KEDR COI core module"
)

if(NOT KERNEL_PART)
    return()
endif(NOT KERNEL_PART)

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_include_directories("${CMAKE_SOURCE_DIR}/include")

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


# The tests
# kedr_coi_test_add_subdirectory (tests)
