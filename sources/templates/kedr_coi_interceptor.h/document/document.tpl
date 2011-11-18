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

// For create factory and creation interceptors
extern struct kedr_coi_factory_interceptor*
<$interceptor.name$>_factory_interceptor_create(
    const char* name,
    size_t factory_operations_field_offset,
    const struct kedr_coi_factory_intermediate* intermediate_operations,
    void (*trace_unforgotten_factory)(void* factory));

extern struct kedr_coi_creation_interceptor*
<$interceptor.name$>_creation_interceptor_create(
    const char* name,
    const struct kedr_coi_creation_intermediate* intermediate_operations,
    void (*trace_unforgotten_watch)(void* id, void* tie));<$endif$>


#ifndef KEDR_COI_CALLBACK_CHECKED
#define KEDR_COI_CALLBACK_CHECKED(func, type) BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(&func), type)) + (char*)(&func)
#endif /* KEDR_COI_CALLBACK_CHECKED */

// Define marcos for interception handlers types(local)
<$block: join(\n)$>

// Declare interception handlers types
<$interception_handlers_types: join(\n)$>

// Undef macros for interception handlers types
<$undef_types_macros: join(\n)$>

// Handlers for concrete operations
<$operation_handlers: join(\n)$>

#endif /* KEDR_COI_INTERCEPTOR_<$interceptor.name$> */