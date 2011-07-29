#ifndef KEDR_COI_PAYLOADS_INTERNAL_H
#define KEDR_COI_PAYLOADS_INTERNAL_H


#include <linux/module.h>

#include <kedr-coi/operations_interception.h>

#include "kedr_coi_instrumentor_internal.h"

/* 
 * Support for 'normal' operations payloads(struct kedr_coi_payload).
 */

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


int
kedr_coi_payloads_container_fix_payloads(
    struct kedr_coi_payloads_container* container);


const struct kedr_coi_replacement*
kedr_coi_payloads_container_get_replacements(
    struct kedr_coi_payloads_container* container);


// Return handlers for operation
int kedr_coi_payloads_container_get_handlers(
    struct kedr_coi_payloads_container* container,
    size_t operation_offset,
    // out parameters
    void* const** pre_handlers,
    void* const** post_handlers);

void
kedr_coi_payloads_container_release_payloads(
    struct kedr_coi_payloads_container* container);

/* 
 * Support for 'foreign' operations payloads(struct kedr_coi_foreign_payload).
 */

struct kedr_coi_foreign_payloads_container;

struct kedr_coi_foreign_payloads_container*
kedr_coi_foreign_payloads_container_create(
    struct kedr_coi_foreign_intermediate* intermediate_operations);

/* 'interceptor_name' is used only for warnings during destroying. */
void
kedr_coi_foreign_payloads_container_destroy(
    struct kedr_coi_foreign_payloads_container* container,
    const char* interceptor_name);

int
kedr_coi_foreign_payloads_container_register_payload(
    struct kedr_coi_foreign_payloads_container* container,
    struct kedr_coi_foreign_payload* payload);

void
kedr_coi_foreign_payloads_container_unregister_payload(
    struct kedr_coi_foreign_payloads_container* container,
    struct kedr_coi_foreign_payload* payload);


int
kedr_coi_foreign_payloads_container_fix_payloads(
    struct kedr_coi_foreign_payloads_container* container);

const struct kedr_coi_replacement*
kedr_coi_foreign_payloads_container_get_replacements(
    struct kedr_coi_foreign_payloads_container* container);

int
kedr_coi_foreign_payloads_container_get_handlers(
    struct kedr_coi_foreign_payloads_container* container,
    // out parameter
    const kedr_coi_foreign_handler_t** handlers);

void
kedr_coi_foreign_payloads_container_release_payloads(
    struct kedr_coi_foreign_payloads_container* container);


#endif /* KEDR_COI_PAYLOADS_INTERNAL_H */