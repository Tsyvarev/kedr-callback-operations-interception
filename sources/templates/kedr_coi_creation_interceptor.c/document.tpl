/*
 * Operations interceptor {{interceptor.name}}.
 */

#include <kedr-coi/operations_interception.h>

<$if header$>
{{ header }}
<$endif$>
<$if implementation_header$>
{{ implementation_header }}
<$endif$>

<$for operation in operations if operation.header$>
{{ operation.header }}
<$endfor$>
<$for operation in operations if operation.implementation_header$>
{{ operation.implementation_header }}
<$endfor$>

static struct kedr_coi_creation_interceptor* interceptor = NULL;

#define OPERATION_OFFSET(operation_name) offsetof({{object.operations_type}}, operation_name)
#define OPERATION_TYPE(operation_name) typeof((({{object.operations_type}}*)0)->operation_name)
#define OPERATION_CHECKED_TYPE(op, operation_name) \
(BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(op), OPERATION_TYPE(operation_name))) + op)

static inline int bind_object(
    {{object.type}}* object,
    {{tie.type}} tie,
    size_t operation_offset,
    struct kedr_coi_creation_intermediate_info* info)
{
    return kedr_coi_factory_interceptor_bind_object_generic(interceptor, object, tie,
        operation_offset, info);
}

<$for operation in operations$>
<$include 'block_block'$>

<$endfor$>



static struct kedr_coi_intermediate intermediate_operations[] =
{
<$for operation in operations$>
    {
        .operation_offset = OPERATION_OFFSET({{operation.name}}),
        .repl = OPERATION_CHECKED_TYPE(&intermediate_repl_{{operation.name}}, {{operation.name}}),
<# Normally, .group and .internal_only attributes are not used for factory interceptors, but them are supported as usual. #>
<$if operation.group_id$>
        .group_id = {{operation.group_id}},
<$endif$>
<$if not operation.default$>
        .internal_only = true,
<$endif$>
    },
<$endfor$>
    {
        .operation_offset = -1,
    }
};

int {{interceptor.name}}_init(
    struct kedr_coi_factory_interceptor* (*factory_interceptor_create_generic)(
        const char* name,
        const struct kedr_coi_intermediate* intermediate_operations))
{
    interceptor = factory_interceptor_create_generic(
        "{{interceptor.name}}",
        intermediate_operations);
    if(interceptor == NULL)
    {
        pr_err("Failed create creation interceptor");
        return -EINVAL;
    }
    
    return 0;
}

void {{interceptor.name}}_trace_unforgotten_object(void (*cb)(const {{id.type}} object))
{
    kedr_coi_factory_interceptor_trace_unforgotten_object(interceptor, (void (*)(const void*))cb);
}


void {{interceptor.name}}_destroy(void)
{
    kedr_coi_factory_interceptor_destroy(interceptor);
}

int {{interceptor.name}}_watch({{id.type}} id, {{tie.type}} tie, const {{object.operations_type}}** p_ops)
{
    return kedr_coi_factory_interceptor_watch_generic(
        interceptor, id, tie, (const void**)(p_ops));
}

int {{interceptor.name}}_forget({{id.type}} id, const {{object.operations_type}}** p_ops)
{
    return kedr_coi_factory_interceptor_forget_generic(
        interceptor, id, (const void**)(p_ops));
}

