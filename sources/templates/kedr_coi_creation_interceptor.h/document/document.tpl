#ifndef KEDR_COI_INTERCEPTOR_<$interceptor.name$>
#define KEDR_COI_INTERCEPTOR_<$interceptor.name$>

#include <kedr-coi/operations_interception.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$>/* Interceptor <$interceptor.name$> */
extern int <$interceptor.name$>_init(
    struct kedr_coi_creation_interceptor* (*creation_interceptor_create)(
        const char* name,
        const struct kedr_coi_creation_intermediate* intermediate_operations,
        void (*trace_unforgotten_watch)(void* id, void* tie)),
    void (*trace_unforgotten_watch)(<$id.type$> id, <$tie.type$> tie));

extern void <$interceptor.name$>_destroy(void);

extern int <$interceptor.name$>_watch(<$id.type$> id, <$tie.type$> tie, const <$operations.type$>** ops_p);
extern int <$interceptor.name$>_forget(<$id.type$> id, const <$operations.type$>** ops_p);

#endif /* KEDR_COI_INTERCEPTOR_<$interceptor.name$> */