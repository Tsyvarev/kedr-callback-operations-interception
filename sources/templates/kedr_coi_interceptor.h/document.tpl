#ifndef KEDR_COI_INTERCEPTOR_{{interceptor.name}}
#define KEDR_COI_INTERCEPTOR_{{interceptor.name}}

#include <kedr-coi/operations_interception.h>

/* Interceptor {{interceptor.name}} */
<$if header$>
{{ header }}
<$endif$>

<$for operation in operations if operation.header$>
{{ operation.header }}
<$endfor$>

int {{interceptor.name}}_init(void);
void {{interceptor.name}}_trace_unforgotten_object(void (*cb)(const {{object.type}}* object));
void {{interceptor.name}}_destroy(void);

int {{interceptor.name}}_payload_register(struct kedr_coi_payload* payload);
int {{interceptor.name}}_payload_unregister(struct kedr_coi_payload* payload);

int {{interceptor.name}}_start(void);
int {{interceptor.name}}_stop(void);

int {{interceptor.name}}_watch({{object.type}}* object);
int {{interceptor.name}}_forget({{object.type}}* object);

int {{interceptor.name}}_forget_norestore({{object.type}}* object);

<$if not interceptor.is_direct$>
// For create factory and creation interceptors
extern struct kedr_coi_factory_interceptor*
{{interceptor.name}}_factory_interceptor_create(
    const char* name,
    size_t factory_operations_field_offset,
    const struct kedr_coi_intermediate* intermediate_operations);

extern struct kedr_coi_creation_interceptor*
{{interceptor.name}}_creation_interceptor_create(
    const char* name,
    const struct kedr_coi_intermediate* intermediate_operations);
<$endif$>


#ifndef KEDR_COI_CALLBACK_CHECKED
#define KEDR_COI_CALLBACK_CHECKED(func, type) BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(&func), type)) + (char*)(&func)
#endif /* KEDR_COI_CALLBACK_CHECKED */

// Interception handlers types.
<$for operation in operations$>
typedef void (*{{interceptor.operations_prefix}}_{{operation.name}}_pre_handler_t)({{operation.args | join(d=", ", attribute="type")}}, struct kedr_coi_operation_call_info* call_info);
typedef void (*{{interceptor.operations_prefix}}_{{operation.name}}_post_handler_t)({{operation.args | join(d=", ", attribute="type")}}<$if operation.returnType$>, {{operation.returnType}}<$endif$>, struct kedr_coi_operation_call_info* call_info);
<$endfor$>

// Handlers for concrete operations
<$for operation in operations$>
#define {{interceptor.operations_prefix}}_{{operation.name}}_pre(pre_handler) { \
    .operation_offset = offsetof(<$include 'operations_type'$>, {{operation.name}}), \
    .func = KEDR_COI_CALLBACK_CHECKED(pre_handler, {{interceptor.operations_prefix}}_{{operation.name}}_pre_handler_t) \
}
#define {{interceptor.operations_prefix}}_{{operation.name}}_post(post_handler) { \
    .operation_offset = offsetof(<$include 'operations_type'$>, {{operation.name}}), \
    .func = KEDR_COI_CALLBACK_CHECKED(post_handler, {{interceptor.operations_prefix}}_{{operation.name}}_post_handler_t) \
}
<$if operation.default$>
#define {{interceptor.operations_prefix}}_{{operation.name}}_pre_external(pre_handler) { \
    .operation_offset = offsetof(<$include 'operations_type'$>, {{operation.name}}), \
    .func = KEDR_COI_CALLBACK_CHECKED(pre_handler, {{interceptor.operations_prefix}}_{{operation.name}}_pre_handler_t), \
    .external = true \
}
#define {{interceptor.operations_prefix}}_{{operation.name}}_post_external(post_handler) { \
    .operation_offset = offsetof(<$include 'operations_type'$>, {{operation.name}}), \
    .func = KEDR_COI_CALLBACK_CHECKED(post_handler, {{interceptor.operations_prefix}}_{{operation.name}}_post_handler_t), \
    .external = true \
}
<$endif$>
<$endfor$>

#endif /* KEDR_COI_INTERCEPTOR_{{interceptor.name}} */
