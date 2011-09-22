/*
 * Operations interceptor <$interceptor.name$>.
 */

#include <kedr-coi/operations_interception.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$><$if concat(implementation_header)$><$implementation_header: join(\n)$>

<$endif$>static struct kedr_coi_foreign_interceptor* interceptor = NULL;

#define OPERATION_OFFSET(operation_name) offsetof(<$operations.type$>, operation_name)
#define OPERATION_TYPE(operation_name) typeof(((<$operations.type$>*)0)->operation_name)
#define OPERATION_CHECKED_TYPE(op, operation_name) \
(BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(op), OPERATION_TYPE(operation_name))) + op)

<$block: join(\n)$>


static struct kedr_coi_foreign_intermediate intermediate_operations[] =
{
<$intermediate_operation: join(\n)$>
    {
        .operation_offset = -1,
    }
};

int <$interceptor.name$>_init(
    struct kedr_coi_foreign_interceptor* (*foreign_interceptor_create)(
        const char* name,
        size_t foreign_operations_field_offset,
        const struct kedr_coi_foreign_intermediate* intermediate_operations,
        void (*trace_unforgotten_object)(void* object)),
    void (*trace_unforgotten_object)(<$prototype_object.type$>* object))
{
    interceptor = foreign_interceptor_create(
        "<$interceptor.name$>",
        offsetof(<$prototype_object.type$>, <$prototype_object.operations_field$>),
        intermediate_operations,
        (void (*)(void*))trace_unforgotten_object);
    if(interceptor == NULL)
    {
        pr_err("Failed create foreign interceptor");
        return -EINVAL;
    }
    
    return 0;
}

void <$interceptor.name$>_destroy(void)
{
    kedr_coi_foreign_interceptor_destroy(interceptor);
}

int <$interceptor.name$>_watch(<$prototype_object.type$> *object)
{
    return kedr_coi_foreign_interceptor_watch(
        interceptor, object);
}

int <$interceptor.name$>_forget(<$prototype_object.type$> *object)
{
    return kedr_coi_foreign_interceptor_forget(
        interceptor, object);
}

int <$interceptor.name$>_forget_norestore(<$prototype_object.type$> *object)
{
    return kedr_coi_foreign_interceptor_forget_norestore(
        interceptor, object);
}
