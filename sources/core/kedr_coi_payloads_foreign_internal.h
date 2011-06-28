#ifndef KEDR_COI_PAYLOADS_FOREIGN_INTERNAL_H
#define KEDR_COI_PAYLOADS_FOREIGN_INTERNAL_H

/* 
 * Support for 'foreign' operations payloads(struct kedr_coi_payload_foreign).
 */

#include <linux/module.h>

#include <kedr-coi/operations_interception.h>

#include "kedr_coi_instrumentor_internal.h"

struct kedr_coi_payload_foreign_container;

struct kedr_coi_payload_foreign_container*
kedr_coi_payload_foreign_container_create(
    struct kedr_coi_intermediate_foreign* intermediate_operations,
    struct kedr_coi_intermediate_foreign_info* intermediate_info);

/* 'interceptor_name' is used only for warnings during destroying. */
void
kedr_coi_payload_foreign_container_destroy(
    struct kedr_coi_payload_foreign_container* container,
    const char* interceptor_name);

int
kedr_coi_payload_foreign_container_register_payload(
    struct kedr_coi_payload_foreign_container* container,
    struct kedr_coi_payload_foreign* payload);

void
kedr_coi_payload_foreign_container_unregister_payload(
    struct kedr_coi_payload_foreign_container* container,
    struct kedr_coi_payload_foreign* payload);


struct kedr_coi_instrumentor_replacement*
kedr_coi_payload_foreign_container_fix_payloads(
    struct kedr_coi_payload_foreign_container* container);

void
kedr_coi_payload_foreign_container_release_payloads(
    struct kedr_coi_payload_foreign_container* container);

#endif /* KEDR_COI_PAYLOADS_FOREIGN_INTERNAL_H */