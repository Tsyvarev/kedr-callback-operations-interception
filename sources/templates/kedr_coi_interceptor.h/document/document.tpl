#ifndef KEDR_COI_INTERCEPTOR_<$interceptor.name$>
#define KEDR_COI_INTERCEPTOR_<$interceptor.name$>

#include <kedr-coi/operations_interception.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$>/* Interceptor <$interceptor.name$> */
extern int <$interceptor.name$>_init(
    void (*trace_unforgotten_object)(<$object.type$>* object));
extern void <$interceptor.name$>_destroy(void);

extern int <$interceptor.name$>_payload_register(struct kedr_coi_payload* payload);
extern int <$interceptor.name$>_payload_unregister(struct kedr_coi_payload* payload);

extern int <$interceptor.name$>_start(void);
extern int <$interceptor.name$>_stop(void);

extern int <$interceptor.name$>_watch(<$object.type$>* object);
extern int <$interceptor.name$>_forget(<$object.type$>* object);

extern int <$interceptor.name$>_forget_norestore(<$object.type$>* object);<$if interceptor.is_direct$><$else$>

// For create foreign interceptors
extern struct kedr_coi_foreign_interceptor*
<$interceptor.name$>_foreign_interceptor_create(
    const char* name,
    size_t foreign_operations_field_offset,
    const struct kedr_coi_foreign_intermediate* intermediate_operations,
    void (*trace_unforgotten_object)(void* object));<$endif$>

// Handlers for concrete operations
<$operation_handlers: join(\n)$>

#endif /* KEDR_COI_INTERCEPTOR_<$interceptor.name$> */