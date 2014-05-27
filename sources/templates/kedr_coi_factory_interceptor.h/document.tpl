#ifndef KEDR_COI_INTERCEPTOR_{{interceptor.name}}
#define KEDR_COI_INTERCEPTOR_{{interceptor.name}}

#include <kedr-coi/operations_interception.h>

<$if header$>
{{ header }}
<$endif$>

<$for operation in operations if operation.header$>
{{ operation.header }}
<$endfor$>

/* Interceptor {{interceptor.name}} */
extern int {{interceptor.name}}_init(
    struct kedr_coi_factory_interceptor* (*factory_interceptor_create)(
        const char* name,
        size_t factory_operations_field_offset,
        const struct kedr_coi_intermediate* intermediate_operations));

void {{interceptor.name}}_trace_unforgotten_object(void (*cb)(const {{factory.type}}* object));

extern void {{interceptor.name}}_destroy(void);

extern int {{interceptor.name}}_watch({{factory.type}} *factory);
extern int {{interceptor.name}}_forget({{factory.type}} *factory);

extern int {{interceptor.name}}_forget_norestore({{factory.type}} *factory);

#endif /* KEDR_COI_INTERCEPTOR_{{interceptor.name}} */
