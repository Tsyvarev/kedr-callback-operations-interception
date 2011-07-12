#ifndef PAYLOADS_FOREIGN_CONTAINER_TEST_COMMON_H
#define PAYLOADS_FOREIGN_CONTAINER_TEST_COMMON_H

#include "kedr_coi_payloads_foreign_internal.h"

struct kedr_coi_test_replacement
{
    const struct kedr_coi_intermediate* intermediate;
    
    void** pre;
    void** post;
};

/*
 * Accept:
 *  -replacements that returned by
 * kedr_coi_payloads_foreign_container_fix_payloads(),
 *  - intermediate information which is passed to
 * the container constructor
 *  -array of intermediates which is passed to
 * the container constructor,
 *  -array of on_create_handlers which should be set.
 * .
 * 
 * Verify:
 *  -all intermediates(and only them) are included into
 * replacements array.
 *  -all given handlers(and only them) are included into
 * information.
 * 
 * Order of replacements and handlers has no sence.
 */
int check_foreign_replacements(const struct kedr_coi_instrumentor_replacement* replacements,
    const struct kedr_coi_intermediate_foreign_info* info,
    const struct kedr_coi_intermediate_foreign* intermediates,
    const kedr_coi_handler_foreign_t* handlers_expected);

#endif /* PAYLOADS_FOREIGN_CONTAINER_TEST_COMMON_H */