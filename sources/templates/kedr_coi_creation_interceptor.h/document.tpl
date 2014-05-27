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
    struct kedr_coi_factory_interceptor* (*factory_interceptor_create_generic)(
        const char* name,
        const struct kedr_coi_intermediate* intermediate_operations));

void {{interceptor.name}}_trace_unforgotten_object(void (*cb)(const {{id.type}} object))

extern void {{interceptor.name}}_destroy(void);

extern int {{interceptor.name}}_watch({{id.type}} id, {{tie.type}} tie, const {{object.operations_type}}** ops_p);
extern int {{interceptor.name}}_forget({{id.type}} id, const {{object.operations_type}}** ops_p);

#endif /* KEDR_COI_INTERCEPTOR_{{interceptor.name}} */
