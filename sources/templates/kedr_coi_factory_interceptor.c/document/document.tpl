/*
 * Operations interceptor <$interceptor.name$>.
 */

#include <kedr-coi/operations_interception.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$><$if concat(implementation_header)$><$implementation_header: join(\n)$>

<$endif$>static struct kedr_coi_factory_interceptor* interceptor = NULL;

#define OPERATION_OFFSET(operation_name) offsetof(<$operations.type$>, operation_name)
#define OPERATION_TYPE(operation_name) typeof(((<$operations.type$>*)0)->operation_name)
#define OPERATION_CHECKED_TYPE(op, operation_name) \
(BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(op), OPERATION_TYPE(operation_name))) + op)

<$block: join(\n)$>


static struct kedr_coi_factory_intermediate intermediate_operations[] =
{
<$intermediate_operation: join(\n)$>
    {
        .operation_offset = -1,
    }
};

int <$interceptor.name$>_init(
    struct kedr_coi_factory_interceptor* (*factory_interceptor_create)(
        const char* name,
        size_t factory_operations_field_offset,
        const struct kedr_coi_factory_intermediate* intermediate_operations,
        void (*trace_unforgotten_factory)(void* factory)),
    void (*trace_unforgotten_factory)(<$factory.type$>* factory))
{
    interceptor = factory_interceptor_create(
        "<$interceptor.name$>",
        offsetof(<$factory.type$>, <$factory.operations_field$>),
        intermediate_operations,
        (void (*)(void*))trace_unforgotten_factory);
    if(interceptor == NULL)
    {
        pr_err("Failed create factory interceptor");
        return -EINVAL;
    }
    
    return 0;
}

void <$interceptor.name$>_destroy(void)
{
    kedr_coi_factory_interceptor_destroy(interceptor);
}

int <$interceptor.name$>_watch(<$factory.type$> *factory)
{
    return kedr_coi_factory_interceptor_watch(
        interceptor, factory);
}

int <$interceptor.name$>_forget(<$factory.type$> *factory)
{
    return kedr_coi_factory_interceptor_forget(
        interceptor, factory);
}

int <$interceptor.name$>_forget_norestore(<$factory.type$> *factory)
{
    return kedr_coi_factory_interceptor_forget_norestore(
        interceptor, factory);
}
