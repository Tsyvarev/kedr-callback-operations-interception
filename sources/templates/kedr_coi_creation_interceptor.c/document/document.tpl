/*
 * Operations interceptor <$interceptor.name$>.
 */

#include <kedr-coi/operations_interception.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$><$if concat(implementation_header)$><$implementation_header: join(\n)$>

<$endif$>static struct kedr_coi_creation_interceptor* interceptor = NULL;

#define OPERATION_OFFSET(operation_name) offsetof(<$operations.type$>, operation_name)
#define OPERATION_TYPE(operation_name) typeof(((<$operations.type$>*)0)->operation_name)
#define OPERATION_CHECKED_TYPE(op, operation_name) \
(BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(op), OPERATION_TYPE(operation_name))) + op)

<$block: join(\n)$>


static struct kedr_coi_creation_intermediate intermediate_operations[] =
{
<$intermediate_operation: join(\n)$>
    {
        .operation_offset = -1,
    }
};

int <$interceptor.name$>_init(
    struct kedr_coi_creation_interceptor* (*creation_interceptor_create)(
        const char* name,
        const struct kedr_coi_creation_intermediate* intermediate_operations,
        void (*trace_unforgotten_watch)(void* id, void* tie)),
    void (*trace_unforgotten_watch)(<$id.type$> id, <$tie.type$> tie))
{
    interceptor = creation_interceptor_create(
        "<$interceptor.name$>",
        intermediate_operations,
        (void (*)(void*, void*))trace_unforgotten_watch);
    if(interceptor == NULL)
    {
        pr_err("Failed create creation interceptor");
        return -EINVAL;
    }
    
    return 0;
}

void <$interceptor.name$>_destroy(void)
{
    kedr_coi_creation_interceptor_destroy(interceptor);
}

int <$interceptor.name$>_watch(<$id.type$> id, <$tie.type$> tie, const <$operations.type$>** p_ops)
{
    return kedr_coi_creation_interceptor_watch(
        interceptor, id, tie, (const void**)(p_ops));
}

int <$interceptor.name$>_forget(<$id.type$> id, const <$operations.type$>** p_ops)
{
    return kedr_coi_creation_interceptor_forget(
        interceptor, id, (const void**)(p_ops));
}
