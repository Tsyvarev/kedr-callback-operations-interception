/*
 * Operations interceptor <$interceptor.name$>.
 */

#include <kedr-coi/operations_interception.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$><$if concat(implementation_header)$><$implementation_header: join(\n)$>

<$endif$>static struct kedr_coi_interceptor* interceptor = NULL;

#define OPERATION_OFFSET(operation_name) offsetof(<$operations_type$>, operation_name)
#define OPERATION_TYPE(operation_name) typeof(((<$operations_type$>*)0)->operation_name)
#define OPERATION_CHECKED_TYPE(op, operation_name) \
(BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(op), OPERATION_TYPE(operation_name))) + op)

<$block: join(\n)$>


static struct kedr_coi_intermediate intermediate_operations[] =
{
    <$intermediate_operation: join(\n\t)$>
    {
        .operation_offset = -1,
    }
};

int <$interceptor.name$>_init(void (*trace_unforgotten_object)(<$object.type$>* object))
{
    interceptor = <$if interceptor.is_direct$>kedr_coi_interceptor_create_direct("<$interceptor.name$>",
       sizeof(<$object.type$>)<$else$>kedr_coi_interceptor_create<$if interceptor.use_copy$>_use_copy<$endif$>("<$interceptor.name$>",
       offsetof(<$object.type$>, <$object.operations_field$>),
       sizeof(<$operations.type$>)<$endif$>,
       intermediate_operations,
       (void (*)(void*))trace_unforgotten_object);

    if(interceptor == NULL)
        return -EINVAL;//TODO: how to report concrete error?
    
    return 0;
}

void <$interceptor.name$>_destroy(void)
{
    kedr_coi_interceptor_destroy(interceptor);
}

int <$interceptor.name$>_payload_register(struct kedr_coi_payload* payload)
{
    return kedr_coi_payload_register(interceptor, payload);
}

void <$interceptor.name$>_payload_unregister(struct kedr_coi_payload* payload)
{
    kedr_coi_payload_unregister(interceptor, payload);
}

int <$interceptor.name$>_start(void)
{
    return kedr_coi_interceptor_start(interceptor);
}

void <$interceptor.name$>_stop(void)
{
    kedr_coi_interceptor_stop(interceptor);
}

int <$interceptor.name$>_watch(<$object.type$> *object)
{
    return kedr_coi_interceptor_watch(interceptor, object);
}

int <$interceptor.name$>_forget(<$object.type$> *object)
{
    return kedr_coi_interceptor_forget(interceptor, object);
}

int <$interceptor.name$>_forget_norestore(<$object.type$> *object)
{
    return kedr_coi_interceptor_forget_norestore(interceptor, object);
}<$if interceptor.is_direct$><$else$>

// For create factory and creation interceptors
extern struct kedr_coi_factory_interceptor*
<$interceptor.name$>_factory_interceptor_create(
    const char* name,
    size_t factory_operations_field_offset,
    const struct kedr_coi_factory_intermediate* _intermediate_operations,
    void (*trace_unforgotten_factory)(void* factory))
{
    return kedr_coi_factory_interceptor_create(
        interceptor,
        name,
        factory_operations_field_offset,
        _intermediate_operations,
        trace_unforgotten_factory);
}

extern struct kedr_coi_creation_interceptor*
<$interceptor.name$>_creation_interceptor_create(
    const char* name,
    const struct kedr_coi_creation_intermediate* _intermediate_operations,
    void (*trace_unforgotten_watch)(void* id, void* tie))
{
    return kedr_coi_creation_interceptor_create(
        interceptor,
        name,
        _intermediate_operations,
        trace_unforgotten_watch);
}<$endif$>
