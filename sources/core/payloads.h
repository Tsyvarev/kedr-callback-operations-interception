/* 
 * Process payloads registration/deregistration and combine their
 * handlers for use in interception.
 */

#ifndef KEDR_COI_PAYLOADS_H
#define KEDR_COI_PAYLOADS_H

#include <kedr-coi/operations_interception.h>
#include "kedr_coi_instrumentor_internal.h"

#include <linux/list.h>

 /*
 * Element of the payload registration.
 */
struct payload_elem
{
    // Element in the list of registered payload elements.
    struct list_head list;
    // Payload itself
    struct kedr_coi_payload* payload;
    // Whether this payload element is fixed
    int is_fixed;
    /*
     *  Element in the list of used payload elements.
     * 
     * NOTE: This list is constant after fix payloads.
     */
    struct list_head list_used;
};


/* Object which control payloads and replacements */
struct operation_payloads
{
    // List of the registered payload elements.
    struct list_head payload_elems;

    // List of all operations available for interception
    struct list_head operations;
    
    /*
     *  Protect list of payloads from concurrent access.
     *  Protect list of foreign interceptors from concurrent access.
     */
    struct mutex m;
    /* 
     * Name of the interceptor for which payloads are registered.
     * Used only in error-reporting.
     */
    const char* interceptor_name;
    /* Whether payloads used for interception */
    int is_used;
    // Next fields are used only when payloads are used.
    
    // List of used payload elements
    struct list_head payload_elems_used;
    // Replacements collected from all used payloads
    struct kedr_coi_replacement* replacements;
};

/* Initialize object with operations payloads.*/
int operation_payloads_init(struct operation_payloads* payloads,
    struct kedr_coi_intermediate* intermediate_operations,
    const char* interceptor_name);

void operation_payloads_destroy(struct operation_payloads* payloads);

/* Add(register) payload */
int operation_payloads_add(struct operation_payloads* payloads,
    struct kedr_coi_payload* payload);
/* Remove(unregister) payload */
int operation_payloads_remove(struct operation_payloads* payloads,
    struct kedr_coi_payload* payload);


/* 
 * Use payloads in interception.
 * 
 * After this call replacements array may be requested from the object.
 * 
 * If 'intercept_all' is not 0, all operations are intercepted in any case,
 * even if no handler set for them.
 */
int operation_payloads_use(struct operation_payloads* payloads,
    int intercept_all);

/* 
 * Returned replacements for given payloads.
 * 
 * May be called only after _use().
 */
const struct kedr_coi_replacement* operation_payloads_get_replacements
    (struct operation_payloads* payloads);

/* 
 * Search interception information for operation which is replaced.
 * 
 * 'is_default' flag should be 0 if need pre- and post- handlers when
 * original operation is NULL, non-zero otherwise.
 * 
 * May be called only after _use().
 */

void operation_payloads_get_interception_info(
    struct operation_payloads* payloads, size_t operation_offset,
    int is_default, void* const** pre_p, void* const** post_p);

/* 
 * Revert using of payloads. 
 */
void operation_payloads_unuse(struct operation_payloads* payloads);


#endif /* KEDR_COI_PAYLOADS_H */