#ifndef KEDR_COI_PAYLOADS_INTERNAL_H
#define KEDR_COI_PAYLOADS_INTERNAL_H

/* 
 * Support for 'normal' operations payloads(struct kedr_coi_payload).
 */

#include <linux/module.h>

#include <kedr-coi/operations_interception.h>

#include "kedr_coi_instrumentor_internal.h"


struct kedr_coi_payloads_container;

struct kedr_coi_payloads_container*
kedr_coi_payloads_container_create(
    struct kedr_coi_intermediate* intermediate_operations);

/* 'interceptor_name' is used only for warnings during destroying */
void
kedr_coi_payloads_container_destroy(
    struct kedr_coi_payloads_container* container,
    const char* interceptor_name);

int
kedr_coi_payloads_container_register_payload(
    struct kedr_coi_payloads_container* container,
    struct kedr_coi_payload* payload);

void
kedr_coi_payloads_container_unregister_payload(
    struct kedr_coi_payloads_container* container,
    struct kedr_coi_payload* payload);


struct kedr_coi_instrumentor_replacement*
kedr_coi_payloads_container_fix_payloads(
    struct kedr_coi_payloads_container* container);

void
kedr_coi_payloads_container_release_payloads(
    struct kedr_coi_payloads_container* container);

#endif /* KEDR_COI_PAYLOADS_INTERNAL_H */