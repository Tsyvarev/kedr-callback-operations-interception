#ifndef FOREIGN_PAYLOADS_CONTAINER_TEST_COMMON_H
#define FOREIGN_PAYLOADS_CONTAINER_TEST_COMMON_H

#include "kedr_coi_payloads_internal.h"

/*
 * Verify that:
 * 1) Container return replacements according to intermediates
 * 2) Container return handlers according to expected ones.
 * 
 * Order of replacements and handlers has no sence.
 */
int check_foreign_payloads_container(struct kedr_coi_foreign_payloads_container* container,
    const struct kedr_coi_foreign_intermediate* intermediates,
    const kedr_coi_foreign_handler_t* handlers_expected);

#endif /* FOREIGN_PAYLOADS_CONTAINER_TEST_COMMON_H */