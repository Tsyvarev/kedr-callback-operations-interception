#ifndef KEDR_COI_INTERCEPTOR_<$interceptor.name$>
#define KEDR_COI_INTERCEPTOR_<$interceptor.name$>

#include <kedr-coi/operations_interception.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$>/* Interceptor <$interceptor.name$> */
extern int <$interceptor.name$>_init(
    struct kedr_coi_foreign_interceptor* (*foreign_interceptor_create)(
        const char* name,
        size_t foreign_operations_field_offset,
        const struct kedr_coi_foreign_intermediate* intermediate_operations,
        void (*trace_unforgotten_object)(void* object)),
    void (*trace_unforgotten_object)(<$prototype_object.type$>* object));

extern void <$interceptor.name$>_destroy(void);

extern int <$interceptor.name$>_watch(<$prototype_object.type$> *object);
extern int <$interceptor.name$>_forget(<$prototype_object.type$> *object);

extern int <$interceptor.name$>_forget_norestore(<$prototype_object.type$> *object);

#endif /* KEDR_COI_INTERCEPTOR_<$interceptor.name$> */