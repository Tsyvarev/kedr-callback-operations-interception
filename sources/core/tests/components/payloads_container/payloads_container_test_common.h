#ifndef PAYLOADS_CONTAINER_TEST_COMMON_H
#define PAYLOADS_CONTAINER_TEST_COMMON_H

#include "kedr_coi_payloads_internal.h"

struct kedr_coi_test_replacement
{
    const struct kedr_coi_intermediate* intermediate;
    
    void** pre;
    void** post;
};

/*
 * Accept replacements that returned by kedr_coi_payloads_container_fix_payloads()
 * and array of intermediates which is expected to be used in replacements.
 * 'replacements_expected' should be terminated with element with intermediate = NULL
 * 
 * Verify that expectations are true, that is:
 * 1. Array of replacements contains all replacement operations from
 *   array of intermediates and only thouse operations
 * 2. Information for all intermediates(kedr_coi_intermediate_info)
 *   contains given handlers and only those handlers.
 * 
 * Order of replacements and order of handlers in intermediate informations
 * has no sence.
 */
int check_replacements(const struct kedr_coi_instrumentor_replacement* replacements,
    const struct kedr_coi_test_replacement* replacements_expected);

#endif /* PAYLOADS_CONTAINER_TEST_COMMON_H */