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
 * Accept payloads container after fixing payloads and array of
 * replacements which is expected to be used by it.
 * 
 * 'replacements_expected' should be terminated with element
 * with intermediate = NULL.
 * 
 * Verify that:
 * 1. Array of replacements returning by container contains
 *   replacements corresponded to expected intermediates.
 * 2. Handlers for replacements, returning by container, correpond to
 *    the expected ones.
 * 
 * Order of replacements and order of handlers in intermediate informations
 * has no sence.
 */
int check_payloads_container(struct kedr_coi_payloads_container* container,
    const struct kedr_coi_test_replacement* replacements_expected);

#endif /* PAYLOADS_CONTAINER_TEST_COMMON_H */